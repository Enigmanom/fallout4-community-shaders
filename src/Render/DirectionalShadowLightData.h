#pragma once

#include "Render/ShadowCascades.h"

#include <DirectXMath.h>

namespace cs::engine
{
	// Mirrors DirectionalShadowLightData in Common\ShadowSampling.hlsli (t98).
	struct alignas(16) DirectionalShadowLightData
	{
		DirectX::XMFLOAT4X4 ShadowProj[2];
		DirectX::XMFLOAT4X4 InvShadowProj[2];
		DirectX::XMFLOAT2 EndSplitDistances;
		DirectX::XMFLOAT2 StartSplitDistances;
	};

	// Keeps the upstream cascade blend divide away from a zero-width band.
	inline constexpr float kMinSunCascadeBlendHalfWidth = 0.5f;

	// Cascades 0 and 1 with the engine overlap; false on a singular matrix.
	[[nodiscard]] bool BuildDirectionalShadowLightData(
		const SunCascadeSnapshot& a_snapshot,
		DirectionalShadowLightData& a_data) noexcept;
}
