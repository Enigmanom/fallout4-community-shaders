#ifndef FO4_SOFT_SUN_SHADOW
#define FO4_SOFT_SUN_SHADOW

#include "FO4/SkylightingConsumer.hlsli"

// Upstream's soft, rim and back sun lobes ride a soft shadow apart from the
// cascade shadow. Probe visibility supplies it where upstream's SHADOW_VIS does.
#if defined(SKYLIGHTING_SHADOW_VIS)
#	define FO4_SOFT_SUN_SHADOW_ACTIVE
#	define FO4_SOFT_SHADOW_BEGIN(softShadow, viewPosition, normalView) FO4_SKYLIGHTING_SOFT_SHADOW(softShadow, viewPosition, normalView)
#endif

#ifdef FO4_SOFT_SUN_SHADOW_ACTIVE
#	ifdef WATER_EFFECTS
#		define FO4_SOFT_SUN_CAUSTICS_TINT causticsMult
#	else
#		define FO4_SOFT_SUN_CAUSTICS_TINT 1.0
#	endif

namespace FO4SoftSunShadow
{
	// FO4: the pass scaled every lobe by shadow; this adds the soft shadow delta.
	float3 LobeDelta(float3 sunColor, float rim, float backfaceWrap, float3 albedoPremult, float forwardBlend, float3 albedo, float3 tint, float shadow, float softShadow)
	{
		float3 softDiffuse = sunColor * (rim + backfaceWrap * albedoPremult + forwardBlend * albedo) * tint;
		return softDiffuse * (softShadow - shadow);
	}
}

// Light-wide multipliers (terrain) scale the soft shadow too.
#	define FO4_SHADOW_LIGHT_MUL(shadow, softShadow, factor) \
		{                                                    \
			const float lightMul = (factor);                 \
			shadow *= lightMul;                              \
			softShadow *= lightMul;                          \
		}
#	define FO4_SOFT_SUN_LOBES(output, shadow, softShadow, sunColor, rim, backfaceWrap, albedoPremult, forwardBlend, albedo) \
		output.diffuse.xyz += FO4SoftSunShadow::LobeDelta(sunColor, rim, backfaceWrap, albedoPremult, forwardBlend, albedo, FO4_SOFT_SUN_CAUSTICS_TINT, shadow, softShadow) / 3.0
#else
#	define FO4_SOFT_SHADOW_BEGIN(softShadow, viewPosition, normalView)
#	define FO4_SHADOW_LIGHT_MUL(shadow, softShadow, factor) shadow *= (factor)
#	define FO4_SOFT_SUN_LOBES(output, shadow, softShadow, sunColor, rim, backfaceWrap, albedoPremult, forwardBlend, albedo)
#endif

// Unshadowed passes fold multipliers into the diffuse sum, so lobes stay apart.
#ifdef SKYLIGHTING_SHADOW_VIS
#	define FO4_SOFT_LOBES_DECLARE(softDiffuse) float3 softDiffuse = 0.0
#	define FO4_SOFT_LOBE(directDiffuse, softDiffuse) softDiffuse
#	define FO4_SOFT_LOBE_SCALE(softDiffuse, factor) softDiffuse *= (factor)
#	define FO4_SOFT_LOBES_APPLY(output, softDiffuse, softShadow) output.diffuse.xyz += (softDiffuse) * (softShadow) / 3.0
#else
#	define FO4_SOFT_LOBES_DECLARE(softDiffuse)
#	define FO4_SOFT_LOBE(directDiffuse, softDiffuse) directDiffuse
#	define FO4_SOFT_LOBE_SCALE(softDiffuse, factor)
#	define FO4_SOFT_LOBES_APPLY(output, softDiffuse, softShadow)
#endif
#endif
