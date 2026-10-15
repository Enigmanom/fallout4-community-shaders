#pragma once

#include <functional>

namespace cs::render
{
	using FrameBoundaryCallback = std::function<void()>;

	// Main thread, immediately before Main::Swap presents the frame.
	// Register only during Load or OnPostPostLoad, before InstallFrameBoundary.
	bool RegisterPreSwap(FrameBoundaryCallback a_callback);

	// Main thread, after Main::Swap returns; the gap between two frames.
	// The FrameDispatcher drains first, then callbacks run in registration order.
	bool RegisterPostSwap(FrameBoundaryCallback a_callback);

	// Hooks the Main::OnIdle -> Main::Swap call; independent of any UI host.
	bool InstallFrameBoundary();

	[[nodiscard]] bool FrameBoundaryInstalled() noexcept;
}
