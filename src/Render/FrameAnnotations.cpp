#include "Render/FrameAnnotations.h"

#include "Log.h"
#include "PCH.h"
#include "Render/Annotation.h"
#include "Render/Engine.h"
#include "Render/RenderHooks.h"
#include "Render/ShaderSubclassHooks.h"
#include "Settings/FeatureConfig.h"

#include "RE/B/BSBloodSplatterShader.h"
#include "RE/B/BSCubeMapCamera.h"
#include "RE/B/BSDFCompositeShader.h"
#include "RE/B/BSDFLightShader.h"
#include "RE/B/BSDFPrePassShader.h"
#include "RE/B/BSDistantTreeShader.h"
#include "RE/B/BSEffectShader.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSLightingShader.h"
#include "RE/B/BSParticleShader.h"
#include "RE/B/BSRenderPass.h"
#include "RE/B/BSShaderAccumulator.h"
#include "RE/B/BSSkyShader.h"
#include "RE/B/BSUtilityShader.h"
#include "RE/B/BSWaterShader.h"
#include "RE/I/ImageSpaceManager.h"

#include <atomic>
#include <concepts>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

// The engine's effect IDs run past magic_enum's default range.
template <>
struct magic_enum::customize::enum_range<RE::ImageSpaceManager::ImageSpaceEffectEnum>
{
	static constexpr int min = 0;
	static constexpr int max = 231;
};

namespace cs::render::frame_annotations
{
	namespace
	{
		auto* L = cs::log::Get("cs.annotations");

		bool g_installed = false;
		std::atomic_bool g_requested{ false };
		std::atomic_bool g_live{ false };
		std::atomic_int g_forces{ 0 };

		// Always: emits whenever the hook exists. Live: also requires the live flag.
		enum class Gate : std::uint8_t
		{
			kAlways,
			kLive
		};

		[[nodiscard]] bool Open(Gate a_gate) noexcept
		{
			return a_gate == Gate::kAlways || Enabled();
		}

		// Annotation-only event; the profiler times leaf passes, never engine containers.
		class Scope
		{
		public:
			Scope(Gate a_gate, std::string_view a_name) noexcept
			{
				if (Open(a_gate))
					_event.emplace(a_name, false);
			}

			template <std::invocable Name>
			Scope(Gate a_gate, Name&& a_name)
			{
				if (Open(a_gate))
					_event.emplace(a_name(), false);
			}

			Scope(const Scope&) = delete;
			Scope& operator=(const Scope&) = delete;

		private:
			std::optional<annotation::ScopedEvent> _event;
		};

		[[nodiscard]] std::string_view ShaderLabel(const RE::BSShader* a_shader) noexcept
		{
			const char* name = a_shader ? a_shader->GetFxpFilename() : nullptr;
			return name ? name : "Unknown";
		}

		// The pixel shader BeginTechnique bound last on this render thread.
		struct BoundTechnique
		{
			const RE::BSShader* shader{};
			std::uint32_t pixelShaderId{};
		};
		thread_local BoundTechnique t_bound;
		thread_local annotation::SpannedEvent t_geometry;

		template <class Shader>
		struct SetupGeometry_Hook
		{
			static constexpr std::size_t size = 0x07;

			static void thunk(RE::BSShader* a_self, RE::BSRenderPass* a_pass)
			{
				if (Enabled() && a_pass) {
					t_geometry.Begin(
						std::format("[{}:{:X}] ({:X}) {}",
							ShaderLabel(a_self),
							t_bound.shader == a_self ? t_bound.pixelShaderId : 0,
							a_pass->passEnum,
							a_pass->geometry ? a_pass->geometry->name.c_str() : ""),
						false);
				}
				func(a_self, a_pass);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		template <class Shader>
		struct RestoreGeometry_Hook
		{
			static constexpr std::size_t size = 0x08;

			static void thunk(RE::BSShader* a_self, RE::BSRenderPass* a_pass)
			{
				func(a_self, a_pass);
				t_geometry.End();
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		template <class Shader>
		void InstallGeometryHooks()
		{
			stl::write_vfunc<Shader, SetupGeometry_Hook<Shader>>();
			stl::write_vfunc<Shader, RestoreGeometry_Hook<Shader>>();
		}

		[[nodiscard]] std::string EffectEventName(std::uint32_t a_effect)
		{
			const auto name = magic_enum::enum_name(static_cast<RE::ImageSpaceManager::ImageSpaceEffectEnum>(a_effect));
			auto label = name.empty() ? std::format("ImageSpaceEffect {}", a_effect) : std::string(name);
			label += label.contains("_CS") ? " Dispatch" : " Draw";
			return label;
		}

		struct RenderEffect_Hook
		{
			static std::uint64_t thunk(RE::ImageSpaceManager* a_this, std::uint32_t a_effect, std::uint32_t a_3, std::uint32_t a_4, void* a_param)
			{
				const Scope scope(Gate::kAlways, [&] { return EffectEventName(a_effect); });
				return func(a_this, a_effect, a_3, a_4, a_param);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct BatchRenderer_RenderBatches_Hook
		{
			static std::uint64_t thunk(RE::BSBatchRenderer* a_this, std::uint32_t a_pass, bool a_alpha, std::uint32_t a_group)
			{
				const Scope scope(Gate::kLive, [&] {
					return std::format("BSBatchRenderer::RenderBatches ({:X})[{}] <{}>", a_pass, a_group, static_cast<unsigned>(a_alpha));
				});
				return func(a_this, a_pass, a_alpha, a_group);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Accumulator_RenderBatches_Hook
		{
			static std::uint64_t thunk(RE::BSShaderAccumulator* a_this, std::uint32_t a_pass, bool a_alpha, std::int32_t a_group)
			{
				const Scope scope(Gate::kLive, [&] {
					return std::format("BSShaderAccumulator::RenderBatches ({:X})[{}] <{}>", a_pass, a_group, static_cast<unsigned>(a_alpha));
				});
				return func(a_this, a_pass, a_alpha, a_group);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		// One pass-list bucket: every pass in it shares the technique.
		struct RenderPassImpl_Hook
		{
			static bool thunk(RE::BSBatchRenderer* a_this, RE::BSRenderPass* a_head, std::uint32_t a_technique, bool a_alpha)
			{
				const Scope scope(Gate::kLive, [&] {
					return std::format("BSBatchRenderer::RenderPass [{}:{:X}] <{}>",
						ShaderLabel(a_head ? a_head->shader : nullptr), a_technique, static_cast<unsigned>(a_alpha));
				});
				return func(a_this, a_head, a_technique, a_alpha);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		enum class FinishPhase : std::uint8_t
		{
			kAll,
			kPreResolveDepth,
			kPostResolveDepth
		};

		template <FinishPhase Phase>
		struct FinishAccumulating_Hook
		{
			static constexpr std::string_view kLabels[]{
				"FinishAccumulating", "FinishAccumulatingPreResolveDepth", "FinishAccumulatingPostResolveDepth"
			};

			static void thunk(RE::BSShaderAccumulator* a_this)
			{
				const Scope scope(Gate::kLive, [&] {
					return std::format("BSShaderAccumulator::{} [{}]",
						kLabels[static_cast<std::size_t>(Phase)], static_cast<std::uint32_t>(a_this->renderMode));
				});
				func(a_this);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Not in CommonLibF4; BSCubeMapCamera::ClickCubeMap, a virtual of the camera.
		struct ClickCubeMap_Hook
		{
			static std::uint64_t thunk(RE::NiAVObject* a_camera, std::int32_t a_faces, bool a_silhouette, bool a_release, bool a_lodOnly)
			{
				const Scope scope(Gate::kAlways, [&] { return std::format("Cubemap {}", a_camera->name.c_str()); });
				return func(a_camera, a_faces, a_silhouette, a_release, a_lodOnly);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		struct Forward_Hook
		{
			static void thunk()
			{
				const Scope scope(Gate::kAlways, "Forward");
				func();
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		// DrawWorld::Imagespace is the engine's Render_UI effect chain.
		struct Imagespace_Hook
		{
			static void thunk()
			{
				const Scope scope(Gate::kAlways, "Imagespace");
				func();
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		// The parameters are opaque here; only the integer registers are forwarded.
		struct DoZPrePass_Hook
		{
			static std::uint64_t thunk(void* a_1, void* a_2, void* a_3, void* a_4, std::int32_t a_5)
			{
				const Scope scope(Gate::kAlways, "Depth");
				return func(a_1, a_2, a_3, a_4, a_5);
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};

		// Bracket callbacks of one engine pass; outermost, so feature work stays inside it.
		constexpr auto kOutermostPre = static_cast<engine::HookPriority>(-1000);
		constexpr auto kOutermostPost = static_cast<engine::HookPriority>(1000);

		template <class RegisterPre, class RegisterPost>
		void BracketPass(RegisterPre a_pre, RegisterPost a_post, std::string_view a_name)
		{
			auto span = std::make_shared<annotation::SpannedEvent>();
			// Post first: a pre without its post would leave the event open.
			if (!a_post([span] { span->End(); }, kOutermostPost) ||
				!a_pre([span, a_name] { span->Begin(a_name, false); }, kOutermostPre))
				L->error("Engine pass '{}' unavailable", a_name);
		}

		template <engine::ShadowLightKind Kind>
		bool PreShadow(engine::RenderHookCallback a_callback, engine::HookPriority a_priority)
		{
			return engine::RegisterPreShadowLightRender(Kind, std::move(a_callback), a_priority);
		}

		template <engine::ShadowLightKind Kind>
		bool PostShadow(engine::RenderHookCallback a_callback, engine::HookPriority a_priority)
		{
			return engine::RegisterPostShadowLightRender(Kind, std::move(a_callback), a_priority);
		}

		void InstallEnginePasses()
		{
			using namespace engine;
			BracketPass(&RegisterPreDeferredPrePass, &RegisterPostDeferredPrePass, "Deferred Prepass");
			BracketPass(&RegisterPreDeferredLightsImpl, &RegisterPostDeferredLightsImpl, "Deferred Lights");
			BracketPass(&RegisterPrePrecipitationOcclusion, &RegisterPostPrecipitationOcclusion, "Precipitation Occlusion");
			BracketPass(&RegisterPreWaterUpdate, &RegisterPostWaterUpdate, "Water Effects");
			BracketPass(&PreShadow<ShadowLightKind::kDirectional>, &PostShadow<ShadowLightKind::kDirectional>, "Directional Light Shadowmaps");
			BracketPass(&PreShadow<ShadowLightKind::kFrustum>, &PostShadow<ShadowLightKind::kFrustum>, "Spot Light Shadowmaps");
			BracketPass(&PreShadow<ShadowLightKind::kParabolic>, &PostShadow<ShadowLightKind::kParabolic>, "Omnidirectional Light Shadowmaps");
		}

		template <class Hook>
		void Detour(REL::VariantID a_id, std::string_view a_name)
		{
			stl::detour_thunk<Hook>(a_id);
			if (Hook::func.address() == 0)
				L->error("Hook failed on {}", a_name);
		}

		template <std::size_t Slot, class Hook>
		void Vfunc(const auto& a_vtable)
		{
			stl::write_vfunc<Slot, Hook>(a_vtable[0]);
		}

		void InstallEngineHooks()
		{
			// OG: IDs.h names 325252, an adjacent function; the engine-facts address is 1309722.
			Detour<RenderEffect_Hook>(REL::VariantID{ 1309722, 2316595, 2316595 }, "ImageSpaceManager::RenderEffect");
			Detour<BatchRenderer_RenderBatches_Hook>(RE::ID::BSBatchRenderer::RenderBatches, "BSBatchRenderer::RenderBatches");
			Detour<RenderPassImpl_Hook>(RE::ID::BSBatchRenderer::RenderPassImpl, "BSBatchRenderer::RenderPassImpl");
			Detour<Accumulator_RenderBatches_Hook>(RE::ID::BSShaderAccumulator::RenderBatches, "BSShaderAccumulator::RenderBatches");
			Detour<ClickCubeMap_Hook>(REL::VariantID{ 593615, 2318829, 2318829 }, "BSCubeMapCamera::ClickCubeMap");
			Detour<Forward_Hook>(RE::ID::DrawWorld::Forward, "DrawWorld::Forward");
			Detour<Imagespace_Hook>(RE::ID::DrawWorld::Imagespace, "DrawWorld::Imagespace");
			Detour<DoZPrePass_Hook>(RE::ID::BSGraphics::Renderer::DoZPrePass, "BSGraphics::Renderer::DoZPrePass");

			// NiAccumulator::FinishAccumulating, then the two BSShaderAccumulator phases.
			Vfunc<0x29, FinishAccumulating_Hook<FinishPhase::kAll>>(RE::VTABLE::BSShaderAccumulator);
			Vfunc<0x2E, FinishAccumulating_Hook<FinishPhase::kPreResolveDepth>>(RE::VTABLE::BSShaderAccumulator);
			Vfunc<0x2F, FinishAccumulating_Hook<FinishPhase::kPostResolveDepth>>(RE::VTABLE::BSShaderAccumulator);

#define CS_HOOK_GEOMETRY(klass, target) InstallGeometryHooks<RE::klass>()
			CS_SHADER_SUBCLASSES(CS_HOOK_GEOMETRY);
#undef CS_HOOK_GEOMETRY
		}

		template <class Id>
		void NameTargets(std::string_view a_prefix, auto&& a_resolve)
		{
			for (const auto id : magic_enum::enum_values<Id>()) {
				const auto value = static_cast<std::int32_t>(id);
				if (value <= 0 || id == Id::kCount)
					continue;
				if (auto* texture = a_resolve(id))
					annotation::SetName(texture, std::format("{}/{}", a_prefix, magic_enum::enum_name(id)));
			}
		}
	}

	bool Installed() noexcept
	{
		return g_installed;
	}

	bool Requested() noexcept
	{
		return g_requested.load(std::memory_order_relaxed);
	}

	bool Enabled() noexcept
	{
		return g_live.load(std::memory_order_relaxed) || g_forces.load(std::memory_order_relaxed) > 0;
	}

	bool SetRequested(bool a_requested)
	{
		const auto saved = feature_config::UpdateTopLevelSection(
			"diagnostics", toml::table{ { "frame_annotations", a_requested } });
		if (!saved) {
			L->warn("Failed to save diagnostics.frame_annotations: {}", saved.error);
			return false;
		}
		g_requested.store(a_requested, std::memory_order_relaxed);
		if (g_installed)
			g_live.store(a_requested, std::memory_order_relaxed);
		return true;
	}

	ScopedForce::ScopedForce() noexcept
	{
		g_forces.fetch_add(1, std::memory_order_relaxed);
	}

	ScopedForce::~ScopedForce() noexcept
	{
		g_forces.fetch_sub(1, std::memory_order_relaxed);
	}

	void Install()
	{
		std::string error;
		const bool requested = feature_config::ParseFrameAnnotations(feature_config::GetRoot(), error);
		if (!error.empty())
			L->warn("diagnostics.{}; using the default", error);
		g_requested.store(requested, std::memory_order_relaxed);
		if (!requested)
			return;
		InstallEnginePasses();
		InstallEngineHooks();
		g_installed = true;
		g_live.store(true, std::memory_order_relaxed);
		L->info("Frame annotations installed");
	}

	void OnDataLoaded()
	{
		if (!g_installed)
			return;
		using RE::BSGraphics::DepthStencilTargetID;
		using RE::BSGraphics::RenderTargetID;
		NameTargets<RenderTargetID>("RenderTarget", [](RenderTargetID a_id) { return engine::GetRenderTargetTexture(a_id); });
		NameTargets<DepthStencilTargetID>("DepthStencil", [](DepthStencilTargetID a_id) { return engine::GetDepthStencilTexture(a_id); });
		if (auto* data = RE::BSGraphics::GetRendererData()) {
			if (auto* texture = data->cubeMapRenderTargets[0].texture)
				annotation::SetName(reinterpret_cast<ID3D11Texture2D*>(texture), "CubeMap/kReflections");
		}
	}

	void OnTechniqueBound(const RE::BSShader* a_shader, std::uint32_t a_pixelShaderId)
	{
		if (!g_installed)
			return;
		t_bound = { a_shader, a_pixelShaderId };
		if (Enabled())
			annotation::SetMarker(std::format("Draw: {}::{:x}", ShaderLabel(a_shader), a_pixelShaderId));
	}
}
