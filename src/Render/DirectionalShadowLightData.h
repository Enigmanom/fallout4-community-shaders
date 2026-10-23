#pragma once

#include "Render/ShadowCascades.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <type_traits>

namespace cs::engine
{
	// SKYLIGHTING_SUN_CASCADES for the layout the Skylighting probe shader reads.
	inline constexpr char kSunCascadesDefine[] = { static_cast<char>('0' + kMaxSunCascades), '\0' };

	// HLSL DirectionalShadowLightData; splits are float2 for N == 2, else float4.
	template <std::size_t N>
	struct alignas(16) DirectionalShadowLightData
	{
		static_assert(N >= 1 && N <= 4);
		using Splits = std::conditional_t<N == 2, DirectX::XMFLOAT2, DirectX::XMFLOAT4>;

		DirectX::XMFLOAT4X4 ShadowProj[N];
		DirectX::XMFLOAT4X4 InvShadowProj[N];
		Splits EndSplitDistances;
		Splits StartSplitDistances;
	};

	// Keeps the upstream cascade blend divide away from a zero-width band.
	inline constexpr float kMinSunCascadeBlendHalfWidth = 0.5f;

	// Fills N cascades with engine overlap B; false on a singular matrix.
	template <std::size_t N>
	[[nodiscard]] bool BuildDirectionalShadowLightData(
		const SunCascadeSnapshot& a_snapshot,
		DirectionalShadowLightData<N>& a_data) noexcept
	{
		a_data = {};
		if (a_snapshot.count == 0 || a_snapshot.count > kMaxSunCascades)
			return false;

		const std::size_t live = (std::min<std::size_t>)(a_snapshot.count, N);
		const float overlap = (std::max)(a_snapshot.overlap, 0.0f);
		std::array<float, N> end{};
		std::array<float, N> start{};
		for (std::size_t i = 0; i < N; ++i) {
			const std::size_t source = (std::min)(i, live - 1);
			const auto& world = a_snapshot.worldToShadow[source];
			// Row-vector engine matrix; column_major HLSL reads the same bytes transposed.
			a_data.ShadowProj[i] = world;
			DirectX::XMVECTOR determinant;
			const auto inverse = DirectX::XMMatrixInverse(&determinant, DirectX::XMLoadFloat4x4(&world));
			const float det = DirectX::XMVectorGetX(determinant);
			if (!std::isfinite(det) || det == 0.0f)
				return false;
			DirectX::XMStoreFloat4x4(&a_data.InvShadowProj[i], inverse);

			end[i] = a_snapshot.splitEnd[source] + overlap;
			start[i] = source == 0 ? 0.0f : (std::max)(a_snapshot.splitEnd[source - 1] - overlap, 0.0f);
		}

		// The two-cascade shader blends across [start[1], end[0]] with a divide.
		if constexpr (N == 2) {
			if (live == 1) {
				// One cascade: the select factor stays 0 below the only end distance.
				end[0] = end[1] + 1.0f;
				start[1] = end[1];
			} else {
				const float half = (std::max)(overlap, kMinSunCascadeBlendHalfWidth);
				end[0] = a_snapshot.splitEnd[0] + half;
				start[1] = (std::max)(a_snapshot.splitEnd[0] - half, 0.0f);
			}
		}

		const auto pack = [](const std::array<float, N>& a_values) {
			if constexpr (N == 2) {
				return DirectX::XMFLOAT2{ a_values[0], a_values[1] };
			} else {
				// The .w slot repeats the last cascade, which is the lit cutoff.
				return DirectX::XMFLOAT4{ a_values[0], a_values[(std::min<std::size_t>)(1, N - 1)], a_values[(std::min<std::size_t>)(2, N - 1)], a_values[N - 1] };
			}
		};
		a_data.EndSplitDistances = pack(end);
		a_data.StartSplitDistances = pack(start);
		return true;
	}
}
