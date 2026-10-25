#pragma once

#include "Host/ShaderCompileOverlayModel.h"

#include <DearModdingUI/API.h>

namespace cs::host
{
	// Live shader-compile progress window; frame thread only.
	class ShaderCompileOverlay
	{
	public:
		[[nodiscard]] bool Observe() noexcept;
		void Draw() const;
		[[nodiscard]] static DMUI_ManagedOverlayOptions ManagedOverlayOptions() noexcept;

	private:
		ShaderCompileOverlayModel _model;
		ShaderCompilePresentation _presentation;
	};
}
