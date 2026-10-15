cbuffer DepthBoundsParameters : register(b0)
{
	float4 TiledParams[3];
};

cbuffer DeferredPerFrame : register(b12)
{
	row_major float4x4 WorldToView;
	row_major float4x4 ViewToClip;
	row_major float4x4 WorldToClip;
	row_major float4x4 ViewToWorld;
	row_major float4x4 ClipToWorld;
	row_major float4x4 ClipToView;
	row_major float4x4 FirstPersonClipToView;
};

Texture2D<float4> MainDepth : register(t0);

struct TileDepthBounds
{
	float Minimum;
	float Maximum;
};

RWStructuredBuffer<TileDepthBounds> TileBounds : register(u5);

groupshared uint MinimumDepthBits;
groupshared uint MaximumDepthBits;

float LinearizeDepth(float depth)
{
	float2 numerator;
	float2 denominator;

	[branch] if (depth <= 0.01)
	{
		depth *= 100.0;
		numerator = FirstPersonClipToView[2].zw;
		denominator = FirstPersonClipToView[3].zw;
	}
	else
	{
		depth = depth * 1.01 - 0.01;
		numerator = ClipToView[2].zw;
		denominator = ClipToView[3].zw;
	}

	float2 depthH = float2(depth, 1.0);
	return dot(numerator, depthH) / dot(denominator, depthH);
}

[numthreads(4, 4, 1)] void main(
	uint groupIndex : SV_GroupIndex,
	uint3 groupId : SV_GroupID,
	uint3 groupThreadId : SV_GroupThreadID) {
	uint2 firstPixel = groupId.xy * 8 + groupThreadId.xy * 2;
	uint2 depthBits = uint2(0xffffffffu, 0u);

	[loop] for (uint sampleY = 0; sampleY < 2; ++sampleY)
	{
		[loop] for (uint sampleX = 0; sampleX < 2; ++sampleX)
		{
			uint sampleBits = asuint(
				MainDepth.Load(uint3(firstPixel + uint2(sampleX, sampleY), 0)).x);
			depthBits.x = min(depthBits.x, sampleBits);
			depthBits.y = max(depthBits.y, sampleBits);
		}
	}

	if (groupIndex == 0) {
		MinimumDepthBits = 0xffffffffu;
		MaximumDepthBits = 0u;
	}
	GroupMemoryBarrierWithGroupSync();

	InterlockedMin(MinimumDepthBits, depthBits.x);
	InterlockedMax(MaximumDepthBits, depthBits.y);
	GroupMemoryBarrierWithGroupSync();

	if (groupIndex == 0) {
		TileDepthBounds bounds;
		bounds.Minimum = LinearizeDepth(asfloat(MinimumDepthBits));
		bounds.Maximum = LinearizeDepth(asfloat(MaximumDepthBits));

		uint tileIndex = groupId.y * (uint)TiledParams[2].z + groupId.x;
		TileBounds[tileIndex] = bounds;
	}
}
