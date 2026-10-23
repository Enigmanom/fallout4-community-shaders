// Engine opaque Z-prepass VS; OFFSET_DEPTH is upstream Utility.hlsl's.
cbuffer PerFrame_CB12 : register(b12)
{
	row_major float4x4 WorldToView;
	row_major float4x4 ViewToClip;
	float4 WorldToClip_row0;
	float4 WorldToClip_row1;
	float4 WorldToClip_row2;
	float4 WorldToClip_row3;
	row_major float4x4 ViewToWorld;
	row_major float4x4 ClipToWorld;
	row_major float4x4 ClipToView;
	row_major float4x4 FirstPersonClipToView;
	float4 HairSpecularScaleAndPower;
	float4 HairSpecularShift;
	float4 WorldWetnessAndRainTransition;
	row_major float4x4 PreviousWorldToClip;
	float4 PosAdjust;
};

cbuffer PerCall_CB2 : register(b2)
{
	float4 World_row0;
	float4 World_row1;
	float4 World_row2;
	float4 World_row3;
};

struct VS_INPUT
{
	float4 position: POSITION0;
};

struct VS_OUTPUT
{
	float4 position: SV_POSITION;
};

VS_OUTPUT main(VS_INPUT input)
{
	float4 position = float4(input.position.xyz, 1.0);
	float4 world = float4(
		dot(float4(World_row0.xyz, World_row0.w - PosAdjust.x), position),
		dot(float4(World_row1.xyz, World_row1.w - PosAdjust.y), position),
		dot(float4(World_row2.xyz, World_row2.w - PosAdjust.z), position),
		dot(World_row3, position));

	VS_OUTPUT output;
	output.position = float4(
		dot(WorldToClip_row0, world),
		dot(WorldToClip_row1, world),
		dot(WorldToClip_row2, world),
		dot(WorldToClip_row3, world));
#if defined(OFFSET_DEPTH)
	output.position.z += 10.0;
#endif
	return output;
}
