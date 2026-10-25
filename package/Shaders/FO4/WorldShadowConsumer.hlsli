// SPDX-License-Identifier: GPL-3.0-only
#ifndef FO4_WORLD_SHADOW_CONSUMER
#define FO4_WORLD_SHADOW_CONSUMER

#if defined(TERRAIN_SHADOWS) || defined(CLOUD_SHADOWS)
#	define FO4_WORLD_SHADOW

#	include "FO4/FO4ShaderData.hlsli"
#	include "FO4/ShadowSamplingConsumer.hlsli"

namespace WorldShadow
{
	// Upstream's GetWorldShadow: terrain and cloud shadows of the sun.
	float GetMultFromViewPosition(float3 viewPosition)
	{
		// FO4: b4 supplies the absolute origin for upstream heightfield sampling.
		return ShadowSampling::GetWorldShadow(FrameBuffer::ViewToWorld(viewPosition), FrameBuffer::CameraPosAdjust.xyz);
	}
}
#endif
#endif
