#pragma once

#include "RE/B/BSShaderProperty.h"
#include "Render/RenderHooks.h"

#include <cstdint>

namespace RE
{
	class BSGeometry;
	class BSLightingShaderProperty;
	class BSShaderAccumulator;
}

namespace cs::engine
{
	struct LightingRenderPassesCall
	{
		RE::BSLightingShaderProperty* property;
		RE::BSGeometry* geometry;
		std::uint32_t renderMode;
		RE::BSShaderAccumulator* accumulator;
	};

	// Runs before the engine builds or reuses the property's pass list.
	using PreLightingRenderPassesCallback = void (*)(const LightingRenderPassesCall&) noexcept;
	// Runs after it, with the list the accumulator is about to register.
	using PostLightingRenderPassesCallback =
		void (*)(const LightingRenderPassesCall&, RE::BSShaderProperty::RenderPassArray*) noexcept;

	// Register only during Load or OnPostPostLoad; callbacks run on accumulation threads.
	bool RegisterPreLightingRenderPasses(PreLightingRenderPassesCallback a_callback, HookPriority a_priority = HookPriority::Default);
	bool RegisterPostLightingRenderPasses(PostLightingRenderPassesCallback a_callback, HookPriority a_priority = HookPriority::Default);
}
