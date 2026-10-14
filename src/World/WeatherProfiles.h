#pragma once

#include "Settings/FeatureConfig.h"
#include "Settings/SettingsMetadata.h"

#include <optional>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

namespace cs::weather
{
	inline constexpr std::string_view kEnabledKey = "__enabled";

	// A feature's per-weather overrides, keyed by engine::WeatherKey.
	class Profiles
	{
	public:
		virtual ~Profiles() = default;

		// The overridable variables, in registration order.
		virtual settings::SchemaView Variables() const = 0;
		// True when the weather has an enabled override.
		virtual bool IsActive(std::string_view a_weather) const = 0;
		// The weather's `__enabled` flag and overridden variables, as written to TOML.
		virtual std::optional<toml::table> Get(std::string_view a_weather) const = 0;
		// Validates and applies live; a null profile removes the weather's override.
		virtual bool Set(std::string_view a_weather, const toml::table* a_profile, std::string& a_error) = 0;
		// Every override, as written to [features.<key>.weather].
		virtual toml::table Serialize() const = 0;
	};

	// Persists the feature's overrides to its weather table.
	feature_config::WriteResult Save(std::string_view a_featureKey, const Profiles& a_profiles);
}
