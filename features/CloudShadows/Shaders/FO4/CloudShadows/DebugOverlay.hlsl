// SPDX-License-Identifier: GPL-3.0-only
#ifdef VSHADER
float4 main(uint vertex : SV_VertexID) : SV_Position
{
	float2 uv = float2((vertex << 1) & 2, vertex & 2);
	return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
#else
#	include "FO4/WorldShadowConsumer.hlsli"

// Drawn over the final image, so the value is not tonemapped or fogged.
float4 main(float4 screenPosition : SV_Position) : SV_Target
{
	uint2 size;
	SharedData::DepthTexture.GetDimensions(size.x, size.y);
	float2 uv = screenPosition.xy / float2(size);
	float depth = SharedData::DepthTexture.Load(int3(uint2(screenPosition.xy), 0)).x;
	float4 view = mul(FrameBuffer::CameraProjUnjitteredInverse,
		float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), depth, 1.0));
	// Same camera-relative position the receivers pass to GetWorldShadow.
	float3 worldPosition = FrameBuffer::ViewToWorld(view.xyz / view.w);
#	ifdef CLOUD_SKY_OVERLAY
	float occlusion = CloudShadows::CloudShadowsTexture.SampleLevel(LinearSampler, normalize(worldPosition), 0);
	return float4(1.0, 0.0, 0.0, occlusion);
#	else
	float shadow = depth < 1.0 ? CloudShadows::GetCloudShadowMult(worldPosition, LinearSampler) : 0.0;
	return float4(shadow.xxx, 1.0);
#	endif
}
#endif
