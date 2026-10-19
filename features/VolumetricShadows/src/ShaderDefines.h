#pragma once
#include "Render/ShaderDefineProvider.h"

#include <cstdint>

namespace cs::features::volumetric_shadows_shader
{
	// PS slots the consumers declare: the map, the shared light block and its sampler.
	inline constexpr std::uint32_t kSharedShadowMapSlot = 18;
	inline constexpr std::uint32_t kShadowLightsSlot = 98;
	inline constexpr std::uint32_t kSamplerSlot = 15;

	inline const engine::ShaderDefineDeclaration kShaderDefines{
		"VOLUMETRIC_SHADOWS",
		{ engine::ShaderInjectionTarget::kBsdfLight, engine::ShaderInjectionTarget::kBsWater }
	};
}
