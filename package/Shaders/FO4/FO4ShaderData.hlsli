#ifndef FO4_SHADER_DATA_HLSLI
#define FO4_SHADER_DATA_HLSLI
#ifdef FO4CS_SUBSTRATE
#	ifdef SAMPLER_MIP_BIAS
// FO4: the bound samplers already carry MipBias.
#		define MipBias SamplerMipBiasCB
#	endif
#	include "Common/SharedData.hlsli"
#	ifdef SAMPLER_MIP_BIAS
#		undef MipBias
namespace SharedData
{
	static const float MipBias = 0.0;
}
#	endif
#	include "FO4/FO4SharedData.hlsli"
namespace FO4SharedData
{
	// FO4: replay vanilla pow(dot(row, (N, 1)), 2.2) exactly, not the linear SH.
	float3 GetAmbientLinear(float3 normalWS)
	{
		float4 normal = float4(normalWS, 1.0);
		float3 ambient = float3(dot(DirectionalAmbient[0], normal), dot(DirectionalAmbient[1], normal), dot(DirectionalAmbient[2], normal));
		return pow(max(0.0, ambient), 2.2);
	}
}
#endif
#endif
