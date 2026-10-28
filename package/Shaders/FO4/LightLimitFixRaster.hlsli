#ifndef FO4_LIGHT_LIMIT_FIX_RASTER_HLSLI
#define FO4_LIGHT_LIMIT_FIX_RASTER_HLSLI

#ifdef LIGHT_LIMIT_FIX
#	include "FO4/LightLimitFixConsumer.hlsli"
// FO4: native raster local lights publish per-draw metadata rather than LLF lists.
cbuffer LightLimitFixDraw : register(b11)
{
	FO4LightLimitFix::PerLightData LightLimitFixLight;
};
#endif

float3 FO4LocalLightColor(float3 nativeColor)
{
#if defined(LIGHT_LIMIT_FIX) && !defined(DIRECTIONAL) && !defined(AMBIENT)
	return FO4LightLimitFix::GetColor(nativeColor, LightLimitFixLight);
#else
	return nativeColor;
#endif
}
float FO4LocalLightAttenuation(float distance, float nativeAttenuation)
{
#ifdef LIGHT_LIMIT_FIX
	return FO4LightLimitFix::GetAttenuation(LightLimitFixLight, distance, nativeAttenuation);
#else
	return nativeAttenuation;
#endif
}
bool FO4LocalLightNegligible(float attenuation)
{
#ifdef LIGHT_LIMIT_FIX
	if (FO4SharedData::EnabledLightLimitFix != 0 &&
		(LightLimitFixLight.lightFlags & LightLimitFix::LightFlags::InverseSquare) != 0)
		return attenuation < 1e-5;
#endif
	return attenuation <= 0.001;
}
#endif
