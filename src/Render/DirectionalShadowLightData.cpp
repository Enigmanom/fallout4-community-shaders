#include "Render/DirectionalShadowLightData.h"

#include <algorithm>
#include <cmath>

namespace cs::engine
{
	bool BuildDirectionalShadowLightData(
		const SunCascadeSnapshot& a_snapshot,
		DirectionalShadowLightData& a_data) noexcept
	{
		a_data = {};
		if (a_snapshot.count == 0 || a_snapshot.count > kMaxSunCascades)
			return false;

		// The block holds two cascades; a third is exposed through the snapshot only.
		const std::size_t cascades = std::min<std::size_t>(a_snapshot.count, 2);
		for (std::size_t i = 0; i < 2; ++i) {
			const auto& world = a_snapshot.worldToShadow[std::min(i, cascades - 1)];
			// Row-vector engine matrix; column_major HLSL reads the same bytes transposed.
			a_data.ShadowProj[i] = world;
			DirectX::XMVECTOR determinant;
			const auto inverse = DirectX::XMMatrixInverse(&determinant, DirectX::XMLoadFloat4x4(&world));
			const float det = DirectX::XMVectorGetX(determinant);
			if (!std::isfinite(det) || det == 0.0f)
				return false;
			DirectX::XMStoreFloat4x4(&a_data.InvShadowProj[i], inverse);
		}

		const float overlap = std::max(a_snapshot.overlap, 0.0f);
		if (cascades == 1) {
			// One cascade: the select factor stays 0 below the only end distance.
			const float end = a_snapshot.splitEnd[0] + overlap;
			a_data.EndSplitDistances = { end + 1.0f, end };
			a_data.StartSplitDistances = { 0.0f, end };
			return true;
		}

		const float half = std::max(overlap, kMinSunCascadeBlendHalfWidth);
		const float blendStart = std::max(a_snapshot.splitEnd[0] - half, 0.0f);
		a_data.EndSplitDistances = { a_snapshot.splitEnd[0] + half, a_snapshot.splitEnd[1] + overlap };
		a_data.StartSplitDistances = { 0.0f, blendStart };
		return true;
	}
}
