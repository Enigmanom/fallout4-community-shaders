#ifndef FO4_LIGHT_LIMIT_FIX_CONSUMER_HLSLI
#define FO4_LIGHT_LIMIT_FIX_CONSUMER_HLSLI

#include "FO4/FO4SharedData.hlsli"

#include "Common/ColorManagement.hlsli"
#include "LightLimitFix/Attenuation.hlsli"

namespace FO4LightLimitFix
{
	float3 GetLinearColor(float3 color, uint lightFlags)
	{
		// FO4: upstream decodes on the CPU; native light color keeps power 2.2.
		return (lightFlags & LightLimitFix::LightFlags::Linear) != 0 ?
		           color :
		           TransferFunctions::Gamma22ToLinear(color);
	}

	float3 GetColor(float3 nativeColor, LightLimitFix::Light data)
	{
		// FO4: the health-gated flag bypasses stale metadata after quarantine.
		[branch] if (FO4SharedData::EnabledLightLimitFix == 0) return nativeColor;
		if ((data.lightFlags & LightLimitFix::LightFlags::Disabled) != 0)
			return 0.0;
		if ((data.lightFlags & LightLimitFix::LightFlags::InverseSquare) == 0)
			return nativeColor;
		float3 color = GetLinearColor(data.color, data.lightFlags);
		// FO4: native local BRDF lacks upstream's Lambert 1/pi, so apply it where inverse-square radiance enters.
		return Color::PointLight(color) * data.fade * Color::BRDFScale;
	}

	float GetAttenuation(LightLimitFix::Light data, float distance, float nativeAttenuation)
	{
		[branch] if (FO4SharedData::EnabledLightLimitFix == 0) return nativeAttenuation;
		if ((data.lightFlags & LightLimitFix::LightFlags::Disabled) != 0)
			return 0.0;
		// FO4: unflagged lights retain the native radial curve, not LLF's regular curve.
		if ((data.lightFlags & LightLimitFix::LightFlags::InverseSquare) == 0)
			return nativeAttenuation;
		return LightLimitFix::GetAttenuation(distance, data);
	}
}
#endif
