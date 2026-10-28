#pragma once

#include "CloudShadowsSettings.h"
#include "Feature.h"
#include "FeatureCategories.h"
#include "Render/RgbaPreview.h"
#include "Render/SharedFeatureData.h"
#include "ShaderDefines.h"

#include <d3d11.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include <winrt/base.h>

namespace cs::features
{
	class CloudShadows : public ShaderFeature<cloud_shadows_shader::kShaderDefines>
	{
	public:
		using Settings = cloud_shadows::Settings;

		static CloudShadows* GetSingleton();

		std::string_view GetName() const override { return "CloudShadows"; }
		std::string_view GetDisplayName() const override { return "Cloud Shadows"; }
		std::string GetCategory() const override { return FeatureCategories::kLighting; }
		std::string GetFeatureSummary() const override
		{
			return "Adds realistic cloud shadows that move across the landscape, creating dynamic lighting changes as clouds pass overhead, enhancing atmospheric immersion.";
		}

		bool Configure(const toml::table& a_config, std::string& a_error) override;
		void Load() override;
		void OnD3D11Ready(IDXGIAdapter* a_adapter, ID3D11Device* a_device) override;
		bool ValidateShaderInjections(std::string& a_error) override;
		void Prepass() override;
		void DrawSettings() override;
		void RestoreDefaultSettings() override;

		bool ProducesTelemetry() const override { return true; }
		void CollectTelemetry(cs::telemetry::Sink& a_sink) const override;
		std::span<const FeatureDebugView> GetDebugViews() const noexcept override;
		void SetDebugView(std::string_view a_view) noexcept override;
		FullscreenDebugData GetFullscreenDebugData() const noexcept override;

		cs::render::CloudShadowsSettings GetCommonBufferData() const;

	private:
		CloudShadows() = default;

		bool SaveSettings() override;
		settings::SettingsBinding GetSettingsBinding() const override { return settings::BindSettings(cloud_shadows::kSchema, _settings); }

		static constexpr std::uint32_t kFaceCount = 6;
		static constexpr std::uint32_t kPreviewWidth = 512;
		static constexpr std::uint32_t kPreviewHeight = 256;

		// The engine cube exists only after the renderer targets are created.
		bool EnsureCube(ID3D11DeviceContext* a_context);
		void ClearFace(ID3D11DeviceContext* a_context, std::uint32_t a_face);
		void ClearAllFaces(ID3D11DeviceContext* a_context);
		// Clears once when the weather has no cloud layer to overwrite the last one.
		void ClearWithoutClouds(ID3D11DeviceContext* a_context);

		// Per cloud draw of the sky shader.
		void BindSkyDraw(ID3D11DeviceContext* a_context);
		bool CaptureCloudDraw(ID3D11DeviceContext* a_context);
		void UnbindExtraTarget(ID3D11DeviceContext* a_context);

		void RenderDebugPreview(ID3D11DeviceContext* a_context);
		FeatureDebugTexture GetOcclusionDebugTexture() const;

		Settings _settings;

		// Render thread only.
		winrt::com_ptr<ID3D11Texture2D> _cube;
		winrt::com_ptr<ID3D11ShaderResourceView> _cubeSRV;
		std::array<winrt::com_ptr<ID3D11RenderTargetView>, kFaceCount> _faceRTV;
		const ID3D11Texture2D* _engineCube = nullptr;
		winrt::com_ptr<ID3D11BlendState> _blend;
		std::uint64_t _clearedSerial = 0;
		bool _clearedWithoutClouds = false;
		bool _resourceFailureLogged = false;

		cs::render::RgbaPreview _preview;
		winrt::com_ptr<ID3D11ComputeShader> _previewCS;
		std::atomic_bool _previewSelected{ false };
		// A CloudShadowsDebugMode value; zero is off.
		std::atomic<std::uint32_t> _fullscreenMode{ 0 };

		std::atomic_bool _registrationsReady{ false };
		std::atomic_bool _injectionsOperational{ false };

		struct Counters
		{
			std::atomic<std::uint64_t> cloudDraws{ 0 };
			std::atomic<std::uint64_t> facesCleared{ 0 };
			std::atomic<std::uint64_t> noCloudClears{ 0 };
			std::atomic<std::uint64_t> framesBound{ 0 };
			std::atomic<std::uint64_t> worldCloudDraws{ 0 };
			std::atomic<std::uint64_t> extraTargetsUnbound{ 0 };
			std::atomic<std::uint64_t> skippedNoCube{ 0 };
			std::atomic<std::uint64_t> skippedNoDepth{ 0 };
			std::atomic<std::uint32_t> cubeSize{ 0 };
			std::atomic<std::uint32_t> cubeMips{ 0 };
		} _counters;
	};
}
