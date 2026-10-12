#pragma once

#include <algorithm>
#include <array>
#include <string_view>

namespace cs::feature_config
{
	inline constexpr std::array<std::string_view, 16> kAllFeatureKeys{
		"ScreenSpaceGI",
		"Skylighting",
		"InverseSquareLighting",
		"ExponentialHeightFog",
		"DynamicCubemaps",
		"ImageBasedLighting",
		"WaterEffects",
		"ScreenSpaceShadows",
		"TerrainShadows",
		"LODBlending",
		"TerrainVariation",
		"MotionVectorFixes",
		"Upscaling",
		"FrameGeneration",
		"PerformanceOverlay",
		"RenderDoc"
	};

	// Activation follows this order, and IBL fails to load before Dynamic Cubemaps.
	static_assert(std::ranges::find(kAllFeatureKeys, "DynamicCubemaps") < std::ranges::find(kAllFeatureKeys, "ImageBasedLighting"));
}
