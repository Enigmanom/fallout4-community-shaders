#pragma once

#include <RE/B/BSLight.h>

namespace cs::engine::local_lights
{
	// FO4: native render falloff (DFLight/DFTiledLighting) at x = distance / radius.
	float RadialFalloff(float a_x, float a_constant, float a_scalar, float a_exponent);
	float ShapeAttenuation(RE::BSLight& a_light, const RE::NiPoint3& a_position);
}
