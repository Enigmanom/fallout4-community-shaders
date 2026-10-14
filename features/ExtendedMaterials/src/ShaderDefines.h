#pragma once
#include "Render/ShaderDefineProvider.h"

namespace cs::features::extended_materials_shader
{
	inline const engine::ShaderDefineDeclaration kShaderDefines{
		"EXTENDED_MATERIALS", { engine::ShaderInjectionTarget::kDeferredPrepass }
	};
}
