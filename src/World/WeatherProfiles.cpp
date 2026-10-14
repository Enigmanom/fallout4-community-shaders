#include "World/WeatherProfiles.h"

#include <array>

namespace cs::weather
{
	feature_config::WriteResult Save(std::string_view a_featureKey, const Profiles& a_profiles)
	{
		const std::array path{ std::string_view("features"), a_featureKey, std::string_view("weather") };
		return feature_config::UpdateOwnedSettingsAt(feature_config::kConfigPath, path, a_profiles.Serialize());
	}
}
