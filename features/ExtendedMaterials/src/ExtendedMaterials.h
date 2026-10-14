#pragma once

#include "Feature.h"
#include "FeatureBuffer.h"
#include "FeatureCategories.h"
#include "ExtendedMaterialsSettings.h"
#include "ShaderDefines.h"

#include <atomic>
#include <string>

namespace cs::features
{
	class ExtendedMaterials : public ShaderFeature<extended_materials_shader::kShaderDefines>
	{
	public:
		static ExtendedMaterials* GetSingleton();

		std::string_view GetName() const override { return "ExtendedMaterials"; }
		std::string_view GetDisplayName() const override { return "Extended Materials"; }
		std::string GetCategory() const override { return FeatureCategories::kLighting; }
		std::string GetFeatureSummary() const override
		{
			return "Extended Materials adds advanced material effects including parallax occlusion mapping.\n"
			       "This feature enhances surface detail and depth perception for more realistic textures.";
		}

		bool Configure(const toml::table& a_config, std::string& a_error) override;
		void Load() override;
		bool ValidateShaderInjections(std::string& a_error) override;
		void DrawSettings() override;
		void RestoreDefaultSettings() override;
		bool ProducesTelemetry() const override { return true; }
		void CollectTelemetry(cs::telemetry::Sink& a_sink) const override;

		cs::ExtendedMaterialsFeatureData GetCommonBufferData() const;

		using Settings = extended_materials::Settings;

	private:
		ExtendedMaterials() = default;

		bool SaveSettings() override;
		settings::SettingsBinding GetSettingsBinding() const override { return settings::BindSettings(extended_materials::kSchema, _settings); }

		Settings _settings;
		std::atomic_bool _registrationsReady{ false };
		std::atomic_bool _injectionsOperational{ false };
	};
}
