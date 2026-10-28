#pragma once

#include "Render/ShaderInjectionTargets.h"

namespace RE
{
	class BSRenderPass;
	class BSShader;
}

namespace cs::engine
{
	// The engine runs SetupGeometry, the draw flush, then RestoreGeometry for each pass.
	struct GeometryObserver
	{
		void (*setup)(RE::BSShader*, RE::BSRenderPass*) noexcept;    // before SetupGeometry
		void (*restore)(RE::BSShader*, RE::BSRenderPass*) noexcept;  // after RestoreGeometry
	};

	// Startup only; the first observer of a target hooks its vtable pair once.
	void ObserveShaderGeometry(ShaderInjectionTarget a_target, GeometryObserver a_observer);
}
