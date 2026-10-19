#pragma once

#include "Feature.h"
#include "FeatureCategories.h"
#include "WeatherOverrideEditor.h"
#include "WeatherPanel.h"

namespace cs::features
{
	// Upstream CS Editor's weather picker, time and override tools.
	class CSEditor final : public Feature
	{
	public:
		static CSEditor* GetSingleton();

		std::string_view GetName() const override { return "CSEditor"; }
		std::string_view GetDisplayName() const override { return "CS Editor"; }
		std::string GetFeatureSummary() const override { return "Weather picker, time controls and per-weather feature overrides."; }
		bool SupportsEnableToggle() const override { return false; }
		std::string GetCategory() const override { return FeatureCategories::kUtility; }

		void Load() override;
		void OnDataLoaded() override;
		void Prepass() override;
		void DrawSettings() override;

	private:
		cs_editor::WeatherPanel _weather;
		cs_editor::WeatherOverrideEditor _overrides;
	};
}
