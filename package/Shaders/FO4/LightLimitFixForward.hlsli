#ifndef FO4_LIGHT_LIMIT_FIX_FORWARD_HLSLI
#define FO4_LIGHT_LIMIT_FIX_FORWARD_HLSLI

#include "FO4/LightLimitFixConsumer.hlsli"
#include "FO4/Permutation.hlsli"

#include "LightLimitFix/LightLimitFix.hlsli"

namespace FO4LightLimitFix
{
	// FO4: spot cone beside each clustered light, indexed like the light list.
	struct Cone
	{
		float3 direction;
		float cosHalfAngle;
		float exponent;
		uint3 pad;
	};

	StructuredBuffer<Cone> Cones : register(t34);

	// FO4: the health-gated flag reverts every forward consumer to its native lanes.
	bool UseClusteredLights()
	{
		return FO4SharedData::EnabledLightLimitFix != 0 &&
		       (Permutation::ExtraShaderDescriptor & Permutation::ExtraFlags::InWorld) != 0;
	}

	// FO4: native effect cone term; exponent 0 means no cone.
	float GetConeAttenuation(uint lightIndex, float3 lightPosition, float3 fragmentPosition)
	{
		Cone cone = Cones[lightIndex];
		[branch] if (cone.exponent == 0) return 1.0;
		float cosine = saturate(dot(normalize(fragmentPosition - lightPosition), cone.direction));
		float t = saturate(1.0 - (1.0 - cosine) / (1.0 - cone.cosHalfAngle));
		return min(pow(t, cone.exponent), 1.0);
	}

	// FO4: unflagged lights keep the native effect curve; inverse-square lights use LLF's.
	float GetEffectAttenuation(float distance, LightLimitFix::Light light)
	{
		if ((light.lightFlags & LightLimitFix::LightFlags::InverseSquare) != 0)
			return LightLimitFix::GetAttenuation(distance, light);
		if ((light.lightFlags & LightLimitFix::LightFlags::Disabled) != 0)
			return 0.0;
		float intensityFactor = saturate(distance * light.invRadius);
		return pow(1.0 - intensityFactor * intensityFactor, 2.2);
	}

	// FO4: the native effect point-light lane, without upstream's pi and 0.5.
	float3 GetEffectLightColor(uint lightIndex, LightLimitFix::Light light, float3 worldPosition)
	{
		float3 lightPosition = light.positionWS.xyz;
		float attenuation = GetEffectAttenuation(length(lightPosition - worldPosition), light);
		return GetLinearColor(light.color, light.lightFlags) * light.fade * attenuation *
		       GetConeAttenuation(lightIndex, lightPosition, worldPosition);
	}

	float3 GetEffectLighting(float3 viewPosition, float3 worldPosition, out uint lightCount)
	{
		float3 result = 0.0;
		lightCount = 0;
		uint clusterIndex = 0;
		if (LightLimitFix::GetClusterIndex(FrameBuffer::ViewToUV(viewPosition), viewPosition.z, clusterIndex)) {
			lightCount = LightLimitFix::lightGrid[clusterIndex].lightCount;
			uint lightOffset = LightLimitFix::lightGrid[clusterIndex].offset;
			[loop] for (uint i = 0; i < lightCount; i++)
			{
				uint clusteredLightIndex = LightLimitFix::lightList[lightOffset + i];
				LightLimitFix::Light light = LightLimitFix::lights[clusteredLightIndex];
				if (LightLimitFix::IsLightIgnored(light) || light.lightFlags & LightLimitFix::LightFlags::ShadowCaster) {
					continue;
				}
				result += GetEffectLightColor(clusteredLightIndex, light, worldPosition);
			}
		}
		return result;
	}
}
#endif
