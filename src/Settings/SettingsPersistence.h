#pragma once

#include "Settings/LiveSettings.h"
#include "Settings/SettingsSchema.h"

#include <spdlog/spdlog.h>

namespace cs::settings
{
	// FO4: never persist a transient comparison variant.
	inline bool PersistenceBlockedByComparison(std::string_view a_featureKey)
	{
		return liveComparisonActive && a_featureKey != "PerformanceOverlay" && a_featureKey != "RenderDoc";
	}

	template <class... Fields>
	bool SaveDelta(
		const Schema<Fields...>& a_schema,
		std::string_view a_featureKey,
		const typename Schema<Fields...>::SettingsType& a_value,
		spdlog::logger& a_log)
	{
		if (PersistenceBlockedByComparison(a_featureKey))
			return false;
		const auto delta = SerializeDelta(a_schema, a_value, typename Schema<Fields...>::SettingsType{});
		const auto result = feature_config::UpdateFeatureOwnedSettings(a_featureKey, delta);
		if (!result)
			a_log.error("Failed to save settings: {}", result.error);
		return result.success;
	}

	inline bool SaveEnabled(std::string_view a_featureKey, bool a_enabled, spdlog::logger& a_log)
	{
		if (PersistenceBlockedByComparison(a_featureKey))
			return false;
		const auto result = feature_config::UpdateFeatureEnabled(a_featureKey, a_enabled);
		if (!result)
			a_log.error("Failed to save enabled state: {}", result.error);
		return result.success;
	}
}
