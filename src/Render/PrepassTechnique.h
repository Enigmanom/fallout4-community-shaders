#pragma once

#include <cstdint>

namespace cs::engine::prepass_technique
{
	// DFPrePass technique (pass enum) bits; names follow the emitted macros.
	inline constexpr std::uint32_t kVertexColor = 1U << 0;
	inline constexpr std::uint32_t kTexture = 1U << 1;
	inline constexpr std::uint32_t kSkinned = 1U << 2;
	inline constexpr std::uint32_t kNormals = 1U << 3;
	inline constexpr std::uint32_t kBinormalTangent = 1U << 4;
	inline constexpr std::uint32_t kLandscape = 1U << 5;
	inline constexpr std::uint32_t kEye = 1U << 6;
	inline constexpr std::uint32_t kGrass = 1U << 7;
	inline constexpr std::uint32_t kAlphaTest = 1U << 8;
	inline constexpr std::uint32_t kLodLandscape = 1U << 9;
	inline constexpr std::uint32_t kTreeAnim = 1U << 10;
	inline constexpr std::uint32_t kLodObjectInstanced = 1U << 11;
	inline constexpr std::uint32_t kCharacterLightMask = 1U << 12;
	inline constexpr std::uint32_t kModelSpaceNormals = 1U << 13;
	inline constexpr std::uint32_t kGlowMap = 1U << 14;
	inline constexpr std::uint32_t kBlend = 1U << 15;
	inline constexpr std::uint32_t kMenuScreen = 1U << 16;
	inline constexpr std::uint32_t kHair = 1U << 17;
	inline constexpr std::uint32_t kSkinTint = 1U << 18;
	inline constexpr std::uint32_t kTessellateDispHeight = 1U << 19;
	inline constexpr std::uint32_t kTessellateDispNormals = 1U << 20;
	inline constexpr std::uint32_t kDismemberment = 1U << 21;
	inline constexpr std::uint32_t kDismembermentMeatCuff = 1U << 22;
	inline constexpr std::uint32_t kPipboyScreen = 1U << 23;
	inline constexpr std::uint32_t kAdditionalAlphaMask = 1U << 24;
	inline constexpr std::uint32_t kLandLodBlend = 1U << 25;
	inline constexpr std::uint32_t kGradientRemap = 1U << 26;
	inline constexpr std::uint32_t kInstanced = 1U << 27;
	inline constexpr std::uint32_t kCombined = 1U << 28;
	inline constexpr std::uint32_t kClipVolume = 1U << 29;
	inline constexpr std::uint32_t kBoneTinting = 1U << 30;
	inline constexpr std::uint32_t kFace = 1U << 31;
	// The vertex stage reads bit 25 as LOD landscape.
	inline constexpr std::uint32_t kLodLandscapeVertex = kLandLodBlend;
}
