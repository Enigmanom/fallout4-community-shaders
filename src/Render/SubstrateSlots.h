#pragma once

#include <cstdint>

namespace cs::render
{
	inline constexpr std::uint32_t kFrameDataSlot = 4;
	inline constexpr std::uint32_t kSharedDataSlot = 5;
	inline constexpr std::uint32_t kFeatureDataSlot = 6;
	inline constexpr std::uint32_t kFO4SharedDataSlot = 7;
	inline constexpr std::uint32_t kSubstrateBufferCount = 4;
	inline constexpr std::uint32_t kCanonicalDepthSlot = 17;
	// Pixel stage; shaders name it LinearSampler (FO4\ShadowSamplingConsumer.hlsli).
	inline constexpr std::uint32_t kLinearClampSamplerSlot = 13;
	inline constexpr std::uint32_t kFullscreenDebugTextureSlot = 61;
}
