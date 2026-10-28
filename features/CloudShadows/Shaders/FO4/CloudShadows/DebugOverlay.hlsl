// SPDX-License-Identifier: GPL-3.0-only
#ifdef VSHADER
float4 main(uint vertex : SV_VertexID) : SV_Position
{
	float2 uv = float2((vertex << 1) & 2, vertex & 2);
	return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
#else
#	include "FO4/WorldShadowConsumer.hlsli"

#	ifdef CLOUD_SKY_OVERLAY
cbuffer Overlay : register(b8)
{
	// Direction to the visible sun disc; w is zero when it is unavailable.
	float4 VisibleSun;
};

// Thin ring of the given angular radius around a direction.
bool OnRing(float3 direction, float3 center, float radius)
{
	return abs(degrees(acos(saturate(dot(direction, center)))) - radius) < 0.2;
}
#	endif

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
	float3 direction = normalize(worldPosition);
	// Green: the direction the receivers look toward. Yellow: the visible sun disc.
	if (OnRing(direction, normalize(SharedData::DirLightDirection.xyz), 1.5))
		return float4(0.0, 1.0, 0.0, 1.0);
	if (VisibleSun.w > 0.0 && OnRing(direction, VisibleSun.xyz, 2.5))
		return float4(1.0, 1.0, 0.0, 1.0);
	float occlusion = CloudShadows::CloudShadowsTexture.SampleLevel(LinearSampler, direction, 0);
	return float4(1.0, 0.0, 0.0, occlusion);
#	else
	float shadow = depth < 1.0 ? CloudShadows::GetCloudShadowMult(worldPosition, LinearSampler) : 0.0;
	return float4(shadow.xxx, 1.0);
#	endif
}
#endif
