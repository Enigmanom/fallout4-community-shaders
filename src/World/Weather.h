#pragma once

#include <cstdint>
#include <format>
#include <span>
#include <string>
#include <string_view>

namespace RE
{
	class TESWeather;
}

namespace cs::engine
{
	struct WeatherSnapshot
	{
		const RE::TESWeather* current = nullptr;
		const RE::TESWeather* previous = nullptr;
		float transitionPct = 1.0f;
	};

	WeatherSnapshot SnapshotWeather() noexcept;

	// Load-order-portable identity: lowercase "0x<local form ID>~<plugin>".
	inline std::string FormatWeatherKey(std::uint32_t a_localID, std::string_view a_plugin)
	{
		auto key = std::format("0x{:X}~{}", a_localID, a_plugin);
		for (auto& c : key)
			if (c >= 'A' && c <= 'Z')
				c = static_cast<char>(c + ('a' - 'A'));
		return key;
	}
	// Empty for null or runtime-created weathers.
	std::string WeatherKey(const RE::TESWeather* a_weather);
	// "EditorID (key)", or the key alone when the editor ID is unknown.
	std::string DescribeWeather(const RE::TESWeather* a_weather);

	// Rebuilds the list once game data is ready; call on the reading thread.
	void LoadWeatherList();
	// Every loaded weather, sorted by description.
	std::span<RE::TESWeather* const> GetAllWeathers();
}
