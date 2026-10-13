#pragma once

#include "Settings/SettingsSchema.h"

#include <array>
#include <cstdint>
#include <string_view>

namespace cs::features::image_based_lighting
{
	// Upstream IBL::Settings without static IBL and the world-map toggle.
	struct Settings
	{
		bool PreserveFogLuminance = false;
		float DALCAmount = 1.0f;
		float EnvIBLScale = 1.0f;
		float SkyIBLScale = 1.0f;
		float EnvIBLSaturation = 1.0f;
		float SkyIBLSaturation = 1.0f;
		float FogAmount = 0.0f;
		// 0: Luminance Ratio, 1: Color Ratio, 2: DALC + Sky, 3: DALC + Sky (Directional)
		std::uint32_t DALCMode = 2;
		bool DisableInInteriors = true;
		bool DisableInLoadingScreen = true;
	};

	inline constexpr std::array<std::string_view, 6> kWeatherVariables{
		"EnvIBLScale", "SkyIBLScale", "EnvIBLSaturation",
		"SkyIBLSaturation", "DALCAmount", "FogAmount"
	};

	inline constexpr settings::Schema kSchema{
		std::tuple{
			settings::Field{ "EnvIBLScale", "Env IBL Scale", &Settings::EnvIBLScale, settings::Range{ 0.0f, 10.0f } },
			settings::Field{ "SkyIBLScale", "Sky IBL Scale", &Settings::SkyIBLScale, settings::Range{ 0.0f, 10.0f } },
			settings::Field{ "EnvIBLSaturation", "Env IBL Saturation", &Settings::EnvIBLSaturation, settings::Range{ 0.0f, 2.0f } },
			settings::Field{ "SkyIBLSaturation", "Sky IBL Saturation", &Settings::SkyIBLSaturation, settings::Range{ 0.0f, 2.0f } },
			settings::Field{ "DALCAmount", "DALC Amount", &Settings::DALCAmount, settings::Range{ 0.0f, 1.0f } },
			settings::Field{ "DALCMode", "DALC Mode", &Settings::DALCMode, settings::Range{ 0u, 3u } },
			settings::Field{ "FogAmount", "Fog Mix", &Settings::FogAmount, settings::Range{ 0.0f, 1.0f } },
			settings::Field{ "PreserveFogLuminance", "Preserve Fog Luminance", &Settings::PreserveFogLuminance },
			settings::Field{ "DisableInInteriors", "Disable in interiors", &Settings::DisableInInteriors },
			settings::Field{ "DisableInLoadingScreen", "Disable in loading screens", &Settings::DisableInLoadingScreen } }
	};
}
