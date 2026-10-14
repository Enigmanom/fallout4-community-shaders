#include "World/Weather.h"

#include "World/EditorIDs.h"

#include "RE/S/Sky.h"
#include "RE/T/TESDataHandler.h"
#include "RE/T/TESFile.h"
#include "RE/T/TESWeather.h"

#include <algorithm>
#include <format>
#include <vector>

namespace cs::engine
{
	WeatherSnapshot SnapshotWeather() noexcept
	{
		WeatherSnapshot s;
		auto* sky = RE::Sky::GetSingleton();
		if (!sky)
			return s;
		s.current = sky->currentWeather;
		s.previous = sky->lastWeather;
		s.transitionPct = std::clamp(sky->currentWeatherPct, 0.0f, 1.0f);
		return s;
	}

	std::string WeatherKey(const RE::TESWeather* a_weather)
	{
		if (!a_weather)
			return {};
		const auto* file = a_weather->GetFile(0);
		if (!file)
			return {};
		return FormatWeatherKey(a_weather->GetFormID() & (file->IsLight() ? 0xFFFu : 0xFFFFFFu), file->GetFilename());
	}

	std::string DescribeWeather(const RE::TESWeather* a_weather)
	{
		if (!a_weather)
			return {};
		auto key = WeatherKey(a_weather);
		if (key.empty())
			key = std::format("{:08X}", a_weather->GetFormID());
		const auto editorID = GetEditorID(a_weather);
		return editorID.empty() ? key : std::format("{} ({})", editorID, key);
	}

	namespace
	{
		std::vector<RE::TESWeather*>& WeatherList()
		{
			static std::vector<RE::TESWeather*> weathers;
			return weathers;
		}
	}

	void LoadWeatherList()
	{
		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler)
			return;
		std::vector<std::pair<std::string, RE::TESWeather*>> sorted;
		for (auto* weather : dataHandler->GetFormArray<RE::TESWeather>()) {
			if (weather)
				sorted.emplace_back(DescribeWeather(weather), weather);
		}
		std::ranges::sort(sorted, {}, &std::pair<std::string, RE::TESWeather*>::first);
		auto& weathers = WeatherList();
		weathers.clear();
		for (const auto& entry : sorted)
			weathers.push_back(entry.second);
	}

	std::span<RE::TESWeather* const> GetAllWeathers()
	{
		return WeatherList();
	}
}
