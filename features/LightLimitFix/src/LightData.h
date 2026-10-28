#pragma once

#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace cs::features::light_limit_fix
{
	enum class LightFlags : std::uint32_t
	{
		kPortalStrict = 1 << 0,
		kShadow = 1 << 1,
		kSimple = 1 << 2,
		kShadowCaster = 1 << 3,
		kLocalShadow = 1 << 4,
		kInitialised = 1 << 8,
		kDisabled = 1 << 9,
		kInverseSquare = 1 << 10,
		kLinear = 1 << 11
	};

	struct PositionOpt
	{
		DirectX::XMFLOAT3 data{};
		std::uint32_t pad0 = 0;
	};

	// Mirrors LightLimitFix::Light in the shared Common.hlsli.
	struct alignas(16) LightData
	{
		DirectX::XMFLOAT3 color{};
		float fade = 1.0f;
		float radius = 0;
		float invRadius = 0;
		float fadeZone = 0;
		float sizeBias = 0;
		PositionOpt positionWS;
		std::array<std::uint32_t, 4> roomFlags{};
		std::uint32_t lightFlags = 0;
		std::uint32_t shadowMaskIndex = 0;
		std::uint32_t localShadowIndex = 0;
		std::uint32_t pad1 = 0;
	};
	static_assert(sizeof(LightData) == 80);
	static_assert(offsetof(LightData, fade) == 12);
	static_assert(offsetof(LightData, radius) == 16);
	static_assert(offsetof(LightData, invRadius) == 20);
	static_assert(offsetof(LightData, fadeZone) == 24);
	static_assert(offsetof(LightData, sizeBias) == 28);
	static_assert(offsetof(LightData, positionWS) == 32);
	static_assert(offsetof(LightData, roomFlags) == 48);
	static_assert(offsetof(LightData, lightFlags) == 64);
	static_assert(offsetof(LightData, shadowMaskIndex) == 68);
	static_assert(offsetof(LightData, localShadowIndex) == 72);
	static_assert(offsetof(LightData, pad1) == 76);
}
