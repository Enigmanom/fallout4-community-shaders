#pragma once

#include "Render/ShadowCascades.h"

#include <cstdint>

struct ID3D11ShaderResourceView;

namespace cs::engine
{
	// Shared sun cascade light data for the t98 DirectionalShadowLights block.
	// Published right after the main sun's Render(7); a zeroed block replaces it
	// before each DeferredLightsImpl, so a frame without a fresh sun render never
	// exposes stale cascades.

	// Call during Load or OnPostPostLoad. No work happens until a consumer acquires.
	[[nodiscard]] bool AcquireSunShadowLights();

	struct SunShadowLightState
	{
		// Result of the latest publication attempt.
		SunCascadeStatus status = SunCascadeStatus::kNoLight;
		// True from a successful publication until the next DeferredLightsImpl.
		bool published = false;
		// Changes on every publication; 0 before the first.
		std::uint64_t generation = 0;
		// Valid while published; the texture is borrowed and only safe to use
		// inside a post-sun-render callback. Cascades 0 and 1 are in the block.
		SunCascadeSnapshot snapshot;
	};

	// Render thread.
	[[nodiscard]] const SunShadowLightState& GetSunShadowLightState() noexcept;

	// Render thread. The published block, or zeroed while unpublished; null before D3D is ready.
	[[nodiscard]] ID3D11ShaderResourceView* GetSunShadowLightsSRV() noexcept;
}
