#pragma once

#include <DirectXMath.h>
#include <d3d11.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace cs::engine
{
	// Consumers size per-cascade data by this; the live count must not exceed it.
	inline constexpr std::size_t kMaxSunCascades = 3;

	// The main sun's cascades; valid only right after its Render(7) returns.
	struct SunCascadeSnapshot
	{
		std::uint32_t count = 0;
		// Borrowed; the engine owns the array target.
		ID3D11Texture2D* texture = nullptr;
		// Row-vector (world, 1) * matrix yields full-texture UV and forward depth.
		std::array<DirectX::XMFLOAT4X4, kMaxSunCascades> worldToShadow{};
		// View-axis distance where each cascade ends.
		std::array<float, kMaxSunCascades> splitEnd{};
		// Engine cascade overlap B: slabs are [near, S0+B] then [Sprev-B, Si+B].
		float overlap = 0.0f;
		std::array<float, kMaxSunCascades> worldUnitsPerTexel{};
		// Sub-rect the cascade rendered into: left, right, top, bottom.
		std::array<std::array<std::int32_t, 4>, kMaxSunCascades> viewport{};
	};

	enum class SunCascadeStatus : std::uint8_t
	{
		kOk,
		kNoLight,
		kUnsupportedCount,
		kNoTarget,
		kInvalid
	};

	inline constexpr std::size_t kSunCascadeStatusCount = 5;

	[[nodiscard]] std::string_view SunCascadeStatusName(SunCascadeStatus a_status) noexcept;
	[[nodiscard]] SunCascadeStatus TryGetSunCascades(SunCascadeSnapshot& a_snapshot) noexcept;
}
