#pragma once

#include <array>
#include <string_view>

namespace cs::FeatureCategories
{
	inline constexpr char kDisplay[] = "Display";
	inline constexpr char kLandscapeAndTextures[] = "Landscape & Textures";
	inline constexpr char kLighting[] = "Lighting";
	inline constexpr char kMaterials[] = "Materials";
	inline constexpr char kOther[] = "Other";
	inline constexpr char kUtility[] = "Utility";
	inline constexpr char kWater[] = "Water";

	inline constexpr std::array<std::string_view, 6> kRenderOrder{
		kDisplay,
		kLandscapeAndTextures,
		kLighting,
		kMaterials,
		kUtility,
		kWater
	};
}
