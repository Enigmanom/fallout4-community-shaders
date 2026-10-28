#ifndef FO4_DEBUG_VIEW_OWNERS_H
#define FO4_DEBUG_VIEW_OWNERS_H

#ifdef __cplusplus
#	include <cstdint>
#endif

namespace FullscreenDebugOwner
{
#ifdef __cplusplus
	inline constexpr std::uint32_t
#else
	static const uint
#endif
		None = 0,
		TerrainShadows = 3,
		WaterEffects = 4,
		Skylighting = 5,
		DynamicCubemaps = 6,
		CloudShadows = 7;
}

namespace DynamicCubemapsDebugMode
{
#ifdef __cplusplus
	inline constexpr std::uint32_t
#else
	static const uint
#endif
		DirectionCube = 1,
		EngineCube = 2;
}

namespace CloudShadowsDebugMode
{
#ifdef __cplusplus
	inline constexpr std::uint32_t
#else
	static const uint
#endif
		SkyOverlay = 1,
		ShadowTerm = 2;
}
#endif
