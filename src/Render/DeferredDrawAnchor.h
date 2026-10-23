#pragma once

#include <cstdint>

namespace cs::engine
{
	struct DeferredDrawAnchorDecision
	{
		bool dispatchInjections = false;
		bool dispatchFullscreenLightCallbacks = false;

		bool operator==(const DeferredDrawAnchorDecision&) const = default;
	};

	constexpr DeferredDrawAnchorDecision SelectDeferredDrawAnchorDecision(
		bool a_insideDeferredLights,
		bool a_insideDeferredComposite,
		bool a_triShapeRoute,
		std::uint32_t a_residualR9d) noexcept
	{
		if (a_insideDeferredComposite)
			return { true, false };
		if (!a_insideDeferredLights)
			return { true, false };

		// Volumes need bindings too; fullscreen callbacks need DrawTriShape's r9d.
		return { true, a_triShapeRoute && a_residualR9d == 2 };
	}
}
