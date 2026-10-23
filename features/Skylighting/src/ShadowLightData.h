#pragma once

#include "Render/ShadowCascades.h"

#include <DirectXMath.h>

namespace cs::features::skylighting
{
	inline constexpr char kSunCascadesDefine[] = { static_cast<char>('0' + cs::engine::kMaxSunCascades), '\0' };

	struct alignas(16) DirectionalShadowLightData
	{
		DirectX::XMFLOAT4X4 ShadowProj[cs::engine::kMaxSunCascades];
		DirectX::XMFLOAT4X4 InvShadowProj[cs::engine::kMaxSunCascades];
		DirectX::XMFLOAT4 EndSplitDistances;
		DirectX::XMFLOAT4 StartSplitDistances;
	};
}
