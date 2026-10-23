#pragma once

#include <cstdint>

namespace cs::engine::prepass_technique
{
	// DFPrePass technique (pass enum) bits.
	inline constexpr std::uint32_t kLandscape = 1U << 5;
	inline constexpr std::uint32_t kLodLandscape = 1U << 9;
	inline constexpr std::uint32_t kBlend = 1U << 15;
	inline constexpr std::uint32_t kLodLandscapeVertex = 1U << 25;
}
