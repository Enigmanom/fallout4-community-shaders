#pragma once

#include "WeatherPanel.h"

#include <string>

namespace RE
{
	class TESWeather;
}

namespace cs::features::cs_editor
{
	// Edits each feature's per-weather overrides for one weather and persists them.
	class WeatherOverrideEditor
	{
	public:
		void Draw(const RE::TESWeather* a_weather, const Theme& a_theme);

	private:
		std::string _error;
	};
}
