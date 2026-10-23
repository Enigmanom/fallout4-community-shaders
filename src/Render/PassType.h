#pragma once

#include <cstdint>

namespace cs::engine::pass_type
{
	// Z-prepass roles inferred from the dropped entries; engine-facts Q27 is open.
	inline constexpr std::uint8_t kDepthPrepassOpaque = 0x19;
	inline constexpr std::uint8_t kDepthPrepassAlphaTest = 0x1A;
	// No RegisterObject_Standard group case, so no geometry group takes it.
	inline constexpr std::uint8_t kUnlisted = 4;
}
