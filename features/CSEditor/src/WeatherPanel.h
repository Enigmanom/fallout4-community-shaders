#pragma once

#include <DearModdingUI/API.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace RE
{
	class Sky;
	class TESWeather;
}

namespace cs
{
	class Feature;
}

namespace cs::features::cs_editor
{
	using Theme = std::optional<DMUI_ThemeColors>;

	// Upstream CSEditor's weather picker, status and information panels.
	class WeatherPanel
	{
	public:
		void DrawTimeControls();
		void DrawStatus(RE::Sky& a_sky, const Theme& a_theme);
		void DrawControls(RE::Sky& a_sky, const Theme& a_theme);
		void DrawInformation(RE::Sky& a_sky, const Theme& a_theme);
		void DrawFeatureAnalysis(const Feature& a_editor);

		// The picked weather, else the current one; overrides edit this weather.
		RE::TESWeather* GetTargetWeather(const RE::Sky& a_sky) const;

	private:
		void UpdateFilteredWeathers();
		void DrawWeatherCombo(const Theme& a_theme);

		std::uint32_t _flagFilter = 0x7F;
		std::uint32_t _lastFlagFilter = 0;
		bool _instantChange = false;
		std::vector<RE::TESWeather*> _filtered;
		std::size_t _loadedCount = 0;
		RE::TESWeather* _selected = nullptr;
		RE::TESWeather* _cachedLastWeather = nullptr;
		std::string _search;
		float _timeScaleSlider = 20.0f;
	};
}
