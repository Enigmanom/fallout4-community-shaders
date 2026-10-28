#ifndef FO4_LIGHT_LIMIT_FIX_FORWARD_HLSLI
#define FO4_LIGHT_LIMIT_FIX_FORWARD_HLSLI

#include "FO4/LightLimitFixConsumer.hlsli"

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

	// FO4: native effect cone term; exponent 0 means no cone.
	float GetConeAttenuation(uint lightIndex, float3 lightPosition, float3 fragmentPosition)
	{
		Cone cone = Cones[lightIndex];
		[branch] if (cone.exponent == 0) return 1.0;
		float cosine = saturate(dot(normalize(fragmentPosition - lightPosition), cone.direction));
		float t = saturate(1.0 - (1.0 - cosine) / (1.0 - cone.cosHalfAngle));
		return min(pow(t, cone.exponent), 1.0);
	}
}
#endif
