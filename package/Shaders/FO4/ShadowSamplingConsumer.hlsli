#ifndef FO4_SHADOW_SAMPLING_CONSUMER
#define FO4_SHADOW_SAMPLING_CONSUMER

#include "FO4/FO4ShaderData.hlsli"

// FO4: the host binds one linear clamp sampler for terrain and VSM lookups.
SamplerState LinearSampler : register(s13);

#include "Common/ShadowSampling.hlsli"
#endif
