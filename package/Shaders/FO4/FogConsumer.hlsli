#ifndef FO4_FOG_CONSUMER
#define FO4_FOG_CONSUMER

#include "FO4/ImageBasedLightingConsumer.hlsli"

// Fog sites compile away unless a fog feature consumes them.
// The IBL tint precedes the height fog, as upstream.
#ifdef IBL
#	define FO4_FOG_VANILLA_COLOR(color) FO4IBL::Fog(color)
#else
#	define FO4_FOG_VANILLA_COLOR(color) (color)
#endif
#ifdef EXPONENTIAL_HEIGHT_FOG
#	include "FO4/ExponentialHeightFogConsumer.hlsli"
// A vanilla fog tint joins here, ahead of the height fog composition.
#	define FO4_FOG_ORIGINAL(name, color) float3 name = color
#	define FO4_FOG_REPLACE(pixelPosition, originalColor, color, opacity) FO4Fog::Replace(pixelPosition, originalColor, color, opacity)
#	define FO4_FOG_REPLACE_FORWARD(screenPosition, originalColor, color, opacity) FO4Fog::ReplaceForward(screenPosition, originalColor, color, opacity)
#else
#	define FO4_FOG_ORIGINAL(name, color)
#	define FO4_FOG_REPLACE(pixelPosition, originalColor, color, opacity)
#	define FO4_FOG_REPLACE_FORWARD(screenPosition, originalColor, color, opacity)
#endif
#endif
