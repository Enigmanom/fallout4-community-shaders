// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 northaxosky
#ifdef FO4CS_SUBSTRATE
#	include "Common/FrameBuffer.hlsli"
#endif

struct VS_INPUT
{
	float4 Position : POSITION0;
	float2 TexCoord : TEXCOORD0;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
// Only widens the output signature; the stock shader never writes it.
#ifdef IMAGESPACE_PASSTHROUGH_TEXCOORD1
	float4 TexCoord1 : TEXCOORD1;
#endif
};

#ifdef FO4CS_SUBSTRATE
// FO4 translation: the engine bakes the render scale into an fp16 UV buffer.
float2 RestoreExactScale(float2 texCoord)
{
	// The fullscreen triangle's far vertices hold twice the scale.
	float2 exactTexCoord = 2.0 * FrameBuffer::DynamicResolutionParams1.xy;
	// Within one fp16 ulp: the engine's float-to-half rounding mode is unknown.
	bool2 isScaled = abs(texCoord - exactTexCoord) <= exactTexCoord * (1.0 / 1024.0);
	return isScaled ? exactTexCoord : texCoord;
}
#endif

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT vsout;
	vsout.Position = float4(input.Position.xyz, 1.0);
#ifdef FO4CS_SUBSTRATE
	vsout.TexCoord = RestoreExactScale(input.TexCoord);
#else
	vsout.TexCoord = input.TexCoord;
#endif
	return vsout;
}
