#ifndef FO4_SUN_SHADOW_FAMILIES
#define FO4_SUN_SHADOW_FAMILIES

// Directional families that render sun cascades; unshadowed passes do not.
#if defined(DIRECTIONAL) && (defined(BSDFLIGHT_PS_DIRSPLITS1) || defined(BSDFLIGHT_PS_DIRSPLITS2) || defined(BSDFLIGHT_PS_DIRSPLITS3))
#	define FO4_SUN_SHADOWED_FAMILY
#endif

#endif
