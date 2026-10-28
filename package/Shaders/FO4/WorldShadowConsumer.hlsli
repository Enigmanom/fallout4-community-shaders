// SPDX-License-Identifier: GPL-3.0-only
#ifndef FO4_WORLD_SHADOW_CONSUMER
#define FO4_WORLD_SHADOW_CONSUMER

#if defined(TERRAIN_SHADOWS) || defined(CLOUD_SHADOWS)
#	define FO4_WORLD_SHADOW

#	include "FO4/Depth.hlsli"
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

	// Fullscreen debug views: the view position of a pixel at its scene depth.
	float3 ViewPositionFromScreenPosition(float2 pixelPosition, out float depth)
	{
		float2 uv = pixelPosition * SharedData::BufferDim.zw;
		uv = FrameBuffer::GetDynamicResolutionUnadjustedScreenPosition(uv);
		depth = SharedData::GetDepth(uv);
		float4 view = mul(FrameBuffer::CameraProjInverse,
			float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), depth, 1.0));
		return view.xyz / view.w;
	}
}
#endif
#endif
