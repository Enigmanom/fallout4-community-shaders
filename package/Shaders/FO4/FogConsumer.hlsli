#ifndef FO4_FOG_CONSUMER
#define FO4_FOG_CONSUMER

#include "FO4/ImageBasedLightingConsumer.hlsli"

// Fog sites compile away unless IBL tints them.
#ifdef IBL
#	define FO4_FOG_VANILLA_COLOR(color) FO4IBL::Fog(color)
#else
#	define FO4_FOG_VANILLA_COLOR(color) (color)
#endif
#endif
