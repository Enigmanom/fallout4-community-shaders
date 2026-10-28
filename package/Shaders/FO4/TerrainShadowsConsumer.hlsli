// SPDX-License-Identifier: GPL-3.0-only
#ifndef FO4_TERRAIN_SHADOWS_CONSUMER
#define FO4_TERRAIN_SHADOWS_CONSUMER

#include "FO4/Depth.hlsli"
#include "FO4/FO4ShaderData.hlsli"
#include "FO4/ForwardPosition.hlsli"
#include "FO4/ShadowSamplingConsumer.hlsli"
#include "FO4/WorldShadowConsumer.hlsli"

namespace TerrainShadows
{
	float GetShadowFromScreenPosition(float3 screenPosition)
	{
		return WorldShadow::GetMultFromViewPosition(FO4Forward::ViewPosition(screenPosition));
	}

#ifdef TERRAIN_SHADOWS_FULLSCREEN_DEBUG
	bool TryGetDebugColorFromViewPosition(float3 viewPosition, out float4 color)
	{
		color = 0.0;
		if (FO4SharedData::DebugOwner != FullscreenDebugOwner::TerrainShadows)
			return false;
		if (!SharedData::terraOccSettings.EnableTerrainShadow || FO4SharedData::DebugMode == 0)
			return false;
		float3 worldPosition = FrameBuffer::ViewToWorld(viewPosition) + FrameBuffer::CameraPosAdjust.xyz;
		float value = GetTerrainShadow(worldPosition, LinearSampler);
		if (FO4SharedData::DebugMode == 2) {
			float2 HeightRange = FO4SharedData::DebugParams.xy;
			float2 DebugHeightRange = FO4SharedData::DebugParams.zw;
			float height = FO4SharedData::DebugTexture.SampleLevel(LinearSampler, GetTerrainShadowUV(worldPosition.xy), 0).x;
			height = lerp(HeightRange.x, HeightRange.y, height);
			value = saturate((height - DebugHeightRange.x) / max(DebugHeightRange.y - DebugHeightRange.x, 1e-3));
		}
		color = float4(value.xxx, 1.0);
		return true;
	}

	bool TryGetDebugColorFromScreenPosition(float2 pixelPosition, out float4 color)
	{
		float depth;
		return TryGetDebugColorFromViewPosition(WorldShadow::ViewPositionFromScreenPosition(pixelPosition, depth), color);
	}
#else
	bool TryGetDebugColorFromViewPosition(float3 viewPosition, out float4 color)
	{
		color = 0.0;
		return false;
	}
#endif
}
#endif
