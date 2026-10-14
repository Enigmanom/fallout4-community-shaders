#pragma once

#include <set>
#include <string>

namespace RE
{
	class TESWeather;
}

namespace cs::features::cs_editor
{
	// Edits and persists each feature's overrides for one weather.
	class WeatherOverrideEditor
	{
	public:
		void Draw(const RE::TESWeather* a_weather);

	private:
		std::string _error;
		// Features with live overrides not yet written to the TOML.
		std::set<std::string, std::less<>> _unsaved;
	};
}
