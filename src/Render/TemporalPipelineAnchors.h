#pragma once

#include "Render/EngineCallSite.h"

namespace cs::render::temporal_anchors
{
	inline constexpr engine::CallSiteAnchor kMainLoopMessageLoopCall{
		.name = "Main::MainLoop -> Main::WindowsMessageLoop",
		.function = RE::ID::Main::Run,
		.offset = { 0x1E, 0x4E, 0x2E },
		.target = RE::ID::Main::Run_WindowsMessageLoop
	};
}
