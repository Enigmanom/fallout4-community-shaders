#pragma once

#include "Settings/SettingsSchema.h"

namespace cs::features::dynamic_cubemaps
{
	struct Settings
	{
		bool enabledSSR = true;
		float materialReflections = 1.0f;
		bool engineCubeLod = true;
	};

	inline constexpr settings::Schema kSchema{
		std::tuple{
			settings::Field{ "enabled_ssr", "Enable all screen-space reflections, including water.", &Settings::enabledSSR },
			settings::Field{ "material_reflections", "Blend from authored material cubemaps to dynamic reflections on environment-mapped surfaces.", &Settings::materialReflections, settings::Range{ 0.0f, 1.0f } },
			settings::Field{ "engine_cube_lod", "Draw LOD land, objects and trees into the engine reflection cube that far water blends toward.", &Settings::engineCubeLod } }
	};
}
