// SPDX-License-Identifier: GPL-3.0-only
#ifndef FO4_CLOUD_SHADOWS_DEBUG
#define FO4_CLOUD_SHADOWS_DEBUG

#include "FO4/WorldShadowConsumer.hlsli"

namespace CloudShadowsDebug
{
	// Both views sample the production cube and sampler.
	bool TryGetDebugColorFromScreenPosition(float2 pixelPosition, out float4 color)
	{
		color = 0.0;
		if (FO4SharedData::DebugOwner != FullscreenDebugOwner::CloudShadows || FO4SharedData::DebugMode == 0)
			return false;
		float depth;
		float3 worldPosition = FrameBuffer::ViewToWorld(WorldShadow::ViewPositionFromScreenPosition(pixelPosition, depth));
		float value = 0.0;
		if (FO4SharedData::DebugMode == CloudShadowsDebugMode::SkyOverlay)
			value = CloudShadows::CloudShadowsTexture.SampleLevel(LinearSampler, normalize(worldPosition), 0);
		else if (depth < 1.0)
			value = CloudShadows::GetCloudShadowMult(worldPosition, LinearSampler);
		color = float4(value.xxx, 1.0);
		return true;
	}
}
#endif
