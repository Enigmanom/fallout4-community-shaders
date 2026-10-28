#ifndef FO4_PERMUTATION_HLSLI
#define FO4_PERMUTATION_HLSLI

// FO4: per-draw forward descriptor; b4 is the frame buffer, so PerShader moves to b8.
namespace Permutation
{
	namespace ExtraFlags
	{
		static const uint InWorld = (1 << 0);
		static const uint SuppressExternalEmittance = (1 << 5);
	}

	cbuffer PerShader : register(b8)
	{
		uint ExtraShaderDescriptor;
		uint3 PerShaderPad;
	};

	// FO4: no effect radius carrier.
	static const float EffectRadius = 0.0;
}
#endif
