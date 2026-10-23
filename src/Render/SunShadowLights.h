#pragma once

#include "Render/ShadowCascades.h"

#include <cstdint>

struct ID3D11ShaderResourceView;

namespace cs::engine
{
	// Sun cascade light block (t98); zeroed before each DeferredLightsImpl.

	// Call in Load or OnPostPostLoad; idle until a consumer acquires.
	[[nodiscard]] bool AcquireSunShadowLights();

	struct SunShadowLightState
	{
		SunCascadeStatus status = SunCascadeStatus::kNoLight;
		bool published = false;
		std::uint64_t generation = 0;
		// Borrowed; use only inside a post-sun-render callback.
		SunCascadeSnapshot snapshot;
	};

	// Render thread.
	[[nodiscard]] const SunShadowLightState& GetSunShadowLightState() noexcept;

	// Zeroed while unpublished; null before D3D is ready.
	[[nodiscard]] ID3D11ShaderResourceView* GetSunShadowLightsSRV() noexcept;
}
