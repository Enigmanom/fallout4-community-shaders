#include "CloudShadows.h"

#include <DearModdingUI/Client.h>

#include <array>
#include <format>
#include <string>
#include <utility>

#include <toml++/toml.hpp>

#include "Log.h"
#include "Menu/Menu.h"
#include "Menu/SettingsEdit.h"
#include "Render/Annotation.h"
#include "Render/Engine.h"
#include "Render/FeatureShaderBindings.h"
#include "Render/FrameBindings.h"
#include "Render/FrameProfiler.h"
#include "Render/ShaderInjection.h"
#include "Render/SharedData.h"
#include "Render/SkyReflectionCube.h"
#include "Settings/SettingsPersistence.h"
#include "Telemetry/Telemetry.h"
#include "Utils/CSUtil.h"

namespace cs::features
{
	namespace
	{
		auto* L = cs::log::Get("cs.feature.cloudshadows");

		constexpr const wchar_t* kPreviewPath = L"Data\\Shaders\\FO4\\CubemapPreviewCS.hlsl";
		constexpr std::string_view kPreviewUnavailable = "Cloud occlusion preview not populated.";
		// RT0 color, RT1 motion; the capture cube takes the next free slot.
		constexpr std::uint32_t kCaptureTargetSlot = 2;
		constexpr std::uint32_t kTargetSlots = 8;
	}

	CloudShadows* CloudShadows::GetSingleton()
	{
		static CloudShadows instance;
		return &instance;
	}

	bool CloudShadows::Configure(const toml::table& a_config, std::string& a_error)
	{
		auto candidate = _settings;
		if (!settings::Parse(cloud_shadows::kSchema, a_config, candidate, a_error))
			return false;
		_settings = candidate;
		_liveSettings = settings::BindLiveSettings(cloud_shadows::kSchema, _settings);
		return true;
	}

	bool CloudShadows::SaveSettings()
	{
		return settings::SaveDelta(cloud_shadows::kSchema, GetConfigKey(), _settings, *L);
	}

	void CloudShadows::Load()
	{
		if (!cs::engine::RegisterFeatureShaderBindings("CloudShadows", *this, [this](cs::engine::ShaderReplacementRegistration& registration) {
				if (registration.targetId == cs::engine::ShaderInjectionTarget::kBsSky)
					registration.bind = [this](ID3D11DeviceContext* a_context) { BindSkyDraw(a_context); };
			})) {
			FailLoad("Cloud Shadows shader contribution registration failed.");
			return;
		}
		if (!cs::engine::RegisterSkyReflectionCubeConsumer("CloudShadows", [this] { return IsEnabled() && IsHealthy(); })) {
			FailLoad("Cloud Shadows needs the sky reflection cube");
			return;
		}
		_registrationsReady.store(true, std::memory_order_release);
	}

	bool CloudShadows::ValidateShaderInjections(std::string& a_error)
	{
		_injectionsOperational.store(false, std::memory_order_release);
		if (!_registrationsReady.load(std::memory_order_acquire)) {
			a_error = "the shader contribution or sky reflection cube did not register";
			return false;
		}
		if (!cs::render::IsSharedDataReady()) {
			a_error = "the shared substrate is unavailable, so the receivers read no sun direction";
			return false;
		}
		if (!cs::engine::ValidateShaderInjectionRoutes("CloudShadows", a_error))
			return false;
		_injectionsOperational.store(true, std::memory_order_release);
		return true;
	}

	void CloudShadows::OnD3D11Ready(IDXGIAdapter*, ID3D11Device* a_device)
	{
		D3D11_BLEND_DESC blendDesc{};
		auto& target = blendDesc.RenderTarget[0];
		target.BlendEnable = TRUE;
		target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
		target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		target.BlendOp = D3D11_BLEND_OP_ADD;
		target.SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
		target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		DX::ThrowIfFailed(a_device->CreateBlendState(&blendDesc, _blend.put()));
		cs::render::annotation::SetName(_blend.get(), "CloudShadows/Blend");

		_preview.Create(a_device, kPreviewWidth, kPreviewHeight, "CloudShadows/Preview");
		_previewCS.attach(static_cast<ID3D11ComputeShader*>(cs::util::CompileShader(kPreviewPath, { { "SINGLE_CHANNEL", "1" } }, "cs_5_0")));
		if (!_previewCS)
			L->warn("The occlusion preview shader failed to compile.");
	}

	bool CloudShadows::EnsureCube(ID3D11DeviceContext* a_context)
	{
		auto* engineCube = cs::engine::GetSkyReflectionCubeTexture();
		auto* device = cs::engine::GetDevice();
		if (!engineCube || !device)
			return false;
		if (_cube && _engineCube == engineCube)
			return true;

		D3D11_TEXTURE2D_DESC desc{};
		engineCube->GetDesc(&desc);
		desc.Format = DXGI_FORMAT_R8_UNORM;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		winrt::com_ptr<ID3D11Texture2D> cube;
		winrt::com_ptr<ID3D11ShaderResourceView> srv;
		std::array<winrt::com_ptr<ID3D11RenderTargetView>, kFaceCount> faces;
		bool created = SUCCEEDED(device->CreateTexture2D(&desc, nullptr, cube.put()));
		if (created) {
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = desc.Format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			srvDesc.TextureCube = { .MostDetailedMip = 0, .MipLevels = desc.MipLevels };
			created = SUCCEEDED(device->CreateShaderResourceView(cube.get(), &srvDesc, srv.put()));
		}
		for (std::uint32_t face = 0; created && face < kFaceCount; ++face) {
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = desc.Format;
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray = { .MipSlice = 0, .FirstArraySlice = face, .ArraySize = 1 };
			created = SUCCEEDED(device->CreateRenderTargetView(cube.get(), &rtvDesc, faces[face].put()));
		}
		if (!created) {
			if (!std::exchange(_resourceFailureLogged, true))
				L->error("Cloud occlusion cube creation failed ({}x{} R8_UNORM).", desc.Width, desc.Height);
			return false;
		}

		cs::render::annotation::SetName(cube.get(), "CloudShadows/Occlusion");
		cs::render::annotation::SetName(srv.get(), "CloudShadows/Occlusion.SRV");
		for (std::uint32_t face = 0; face < kFaceCount; ++face)
			cs::render::annotation::SetName(faces[face].get(), std::format("CloudShadows/Occlusion.RTV{}", face));
		_cube = std::move(cube);
		_cubeSRV = std::move(srv);
		_faceRTV = std::move(faces);
		_engineCube = engineCube;
		_clearedSerial = 0;
		// Faces the engine has not rendered yet read as unshadowed.
		ClearAllFaces(a_context);
		_counters.cubeSize.store(desc.Width, std::memory_order_relaxed);
		_counters.cubeMips.store(desc.MipLevels, std::memory_order_relaxed);
		L->info("occlusion cube: {0}x{0} R8_UNORM, {1} mips", desc.Width, desc.MipLevels);
		return true;
	}

	void CloudShadows::ClearFace(ID3D11DeviceContext* a_context, std::uint32_t a_face)
	{
		constexpr float kClear[4]{};
		a_context->ClearRenderTargetView(_faceRTV[a_face].get(), kClear);
	}

	void CloudShadows::ClearAllFaces(ID3D11DeviceContext* a_context)
	{
		for (std::uint32_t face = 0; face < kFaceCount; ++face)
			ClearFace(a_context, face);
	}

	void CloudShadows::ClearWithoutClouds(ID3D11DeviceContext* a_context)
	{
		const auto* sky = RE::Sky::GetSingleton();
		if (!sky || sky->mode.get() != RE::Sky::Mode::kFull || !sky->currentClimate)
			return;
		// No cloud layer draws, so nothing would overwrite the last weather's occlusion.
		if (sky->clouds && sky->clouds->numLayers != 0) {
			_clearedWithoutClouds = false;
			return;
		}
		if (_clearedWithoutClouds || !_cube)
			return;
		cs::render::annotation::ScopedEvent timing("CloudShadows/ClearNoClouds");
		ClearAllFaces(a_context);
		_clearedWithoutClouds = true;
		_counters.noCloudClears.fetch_add(1, std::memory_order_relaxed);
	}

	void CloudShadows::BindSkyDraw(ID3D11DeviceContext* a_context)
	{
		if (!a_context || !cs::engine::ActiveShaderInjectionVariantHasDefine(cs::engine::ShaderInjectionTarget::kBsSky, "CLOUDS"))
			return;
		const bool capturing = IsHealthy() && IsEnabled() && _injectionsOperational.load(std::memory_order_acquire);
		if (capturing && CaptureCloudDraw(a_context))
			return;
		UnbindExtraTarget(a_context);
	}

	bool CloudShadows::CaptureCloudDraw(ID3D11DeviceContext* a_context)
	{
		const auto capture = cs::engine::GetSkyReflectionCubeCapture();
		if (!capture)
			return false;
		if (!EnsureCube(a_context)) {
			_counters.skippedNoCube.fetch_add(1, std::memory_order_relaxed);
			return false;
		}
		auto* depth = cs::engine::GetDepthStencilDepthSRV(RE::BSGraphics::DepthStencilTargetID::kCubemapReflections);
		if (!depth) {
			_counters.skippedNoDepth.fetch_add(1, std::memory_order_relaxed);
			return false;
		}

		cs::render::annotation::SetMarker("CloudShadows/CloudDraw");
		// The face composite restarts at the first cloud draw of each face render.
		if (std::exchange(_clearedSerial, capture->serial) != capture->serial) {
			ClearFace(a_context, capture->face);
			_counters.facesCleared.fetch_add(1, std::memory_order_relaxed);
		}

		// The draw scope restores the targets, blend state and depth slot.
		cs::engine::CaptureShaderInjectionOutputs(a_context);
		ID3D11RenderTargetView* color = nullptr;
		a_context->OMGetRenderTargets(1, &color, nullptr);
		ID3D11RenderTargetView* const targets[]{ color, nullptr, _faceRTV[capture->face].get() };
		static_assert(std::size(targets) == kCaptureTargetSlot + 1);
		// FO4: the face depth is read as a texture, so it cannot stay bound for testing.
		a_context->OMSetRenderTargets(static_cast<UINT>(std::size(targets)), targets, nullptr);
		if (color)
			color->Release();
		constexpr float kBlendFactor[4]{ 1.0f, 1.0f, 1.0f, 1.0f };
		a_context->OMSetBlendState(_blend.get(), kBlendFactor, 0xFFFFFFFFu);
		cs::engine::OverrideInjectionFrameShaderResource(a_context, cloud_shadows_shader::kCloudDepthSlot, depth);
		_counters.cloudDraws.fetch_add(1, std::memory_order_relaxed);
		return true;
	}

	void CloudShadows::UnbindExtraTarget(ID3D11DeviceContext* a_context)
	{
		// The cloud shader declares the capture output; no other draw may receive it.
		ID3D11RenderTargetView* targets[kTargetSlots]{};
		ID3D11DepthStencilView* depth = nullptr;
		a_context->OMGetRenderTargets(kTargetSlots, targets, &depth);
		if (targets[kCaptureTargetSlot]) {
			cs::engine::CaptureShaderInjectionOutputs(a_context);
			targets[kCaptureTargetSlot]->Release();
			targets[kCaptureTargetSlot] = nullptr;
			a_context->OMSetRenderTargets(kTargetSlots, targets, depth);
			_counters.extraTargetsUnbound.fetch_add(1, std::memory_order_relaxed);
		}
		for (auto* target : targets) {
			if (target)
				target->Release();
		}
		if (depth)
			depth->Release();
		if (!cs::engine::GetSkyReflectionCubeCapture())
			_counters.worldCloudDraws.fetch_add(1, std::memory_order_relaxed);
	}

	void CloudShadows::Prepass()
	{
		auto* context = cs::engine::GetImmediateContext();
		if (!context)
			return;
		if (IsEnabled() && _injectionsOperational.load(std::memory_order_acquire) && EnsureCube(context))
			ClearWithoutClouds(context);
		// Disabled, the opacity is zero and the lookup multiplies by one.
		ID3D11ShaderResourceView* const cube = _cubeSRV.get();
		cs::engine::BindFrameShaderResources(context, cs::engine::ShaderStage::kPixel, cloud_shadows_shader::kCloudShadowsSlot, 1, &cube);
		_counters.framesBound.fetch_add(1, std::memory_order_relaxed);
		RenderDebugPreview(context);
	}

	void CloudShadows::RenderDebugPreview(ID3D11DeviceContext* a_context)
	{
		if (!_previewSelected.load(std::memory_order_acquire) || !_previewCS || !_cubeSRV)
			return;
		cs::render::annotation::ScopedEvent timing("CloudShadows/Preview");
		_preview.Convert(a_context, _previewCS.get(), _cubeSRV.get(), cs::render::GetLinearClampSampler());
	}

	cs::render::CloudShadowsSettings CloudShadows::GetCommonBufferData() const
	{
		return { .Opacity = _settings.Opacity };
	}

	std::span<const FeatureDebugView> CloudShadows::GetDebugViews() const noexcept
	{
		static constexpr std::array views{
			FeatureDebugView{
				.id = "cloud_occlusion",
				.label = "Cloud occlusion",
				.kind = FeatureDebugViewKind::kTexturePreview,
				.textureProvider = [](const Feature& a_feature) {
					return static_cast<const CloudShadows&>(a_feature).GetOcclusionDebugTexture();
				} }
		};
		return views;
	}

	void CloudShadows::SetDebugView(std::string_view a_view) noexcept
	{
		_preview.Invalidate();
		_previewSelected.store(a_view == "cloud_occlusion", std::memory_order_release);
	}

	FeatureDebugTexture CloudShadows::GetOcclusionDebugTexture() const
	{
		if (!_previewSelected.load(std::memory_order_acquire))
			return { .unavailableText = kPreviewUnavailable };
		return _preview.GetDebugTexture(kPreviewUnavailable);
	}

	void CloudShadows::DrawSettings()
	{
		settings::SettingsEdit edit{ *this };
		const auto range = cloud_shadows::kSchema.EditRange(&Settings::Opacity);
		edit.Continuous(dmui::ui::SliderScalar("Opacity##Opacity", &_settings.Opacity, &range.min, &range.max, "%.1f"));
		if (const dmui::TooltipScope tooltip{ dmui::ui::HoveredFlags::kNone }; tooltip.Visible())
			dmui::ui::Text("%s", "Higher values make cloud shadows darker.");
		Menu::Get().DrawDebugViewSelector(*this);
	}

	void CloudShadows::RestoreDefaultSettings()
	{
		_settings = Settings{};
		SaveSettings();
	}

	void CloudShadows::CollectTelemetry(cs::telemetry::Sink& a_sink) const
	{
		const auto count = [](const std::atomic<std::uint64_t>& a_value) { return a_value.load(std::memory_order_relaxed); };
		const auto size = _counters.cubeSize.load(std::memory_order_relaxed);
		a_sink
			.Field("operational", _injectionsOperational.load(std::memory_order_relaxed))
			.Field("cloud_draws_captured", count(_counters.cloudDraws))
			.Field("faces_cleared", count(_counters.facesCleared))
			.Field("no_cloud_clears", count(_counters.noCloudClears))
			.Field("frames_bound", count(_counters.framesBound))
			.Field("world_cloud_draws", count(_counters.worldCloudDraws))
			.Field("extra_targets_unbound", count(_counters.extraTargetsUnbound))
			.Field("skipped_no_cube", count(_counters.skippedNoCube))
			.Field("skipped_no_depth", count(_counters.skippedNoDepth))
			.Dimensions("cube", size, size)
			.Field("cube_mips", _counters.cubeMips.load(std::memory_order_relaxed));
		cs::render::profiling::CollectPassTimings(a_sink, "CloudShadows/");
	}

	namespace
	{
		struct AutoRegister
		{
			AutoRegister() { FeatureManager::Get().Register(CloudShadows::GetSingleton()); }
		};
		static AutoRegister _autoRegister;
	}
}
