#pragma once
#include "Render/ShaderDefineProvider.h"
#include "Render/ShaderInjectionDefines.h"

namespace cs::features::upscaling
{
	// Owned targets drawn inside a SamplerBias override window.
	inline const engine::ShaderDefineDeclaration kSamplerBiasShaderDefines{
		"SAMPLER_MIP_BIAS",
		{ engine::ShaderInjectionTarget::kDeferredPrepass, engine::ShaderInjectionTarget::kDistantTree,
			engine::ShaderInjectionTarget::kEffect }
	};

	// Imagespace fullscreen VS draws read the exact render scale from b4.
	inline const engine::ShaderDefineDeclaration kImageSpaceUvShaderDefines{
		engine::shader_injection_defines::kSubstrate,
		{ engine::ShaderInjectionTarget::kImageSpace },
		{},
		engine::ShaderInjectionTarget::kCount,
		engine::ShaderStageBit(engine::ShaderStage::kVertex)
	};
}
