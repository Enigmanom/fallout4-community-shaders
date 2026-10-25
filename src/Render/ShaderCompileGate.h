#pragma once

#include <string_view>

namespace cs::engine
{
	// Logs one summary line per finished compile batch; call once at startup.
	void InstallShaderCompileSummaryLog();

	// Main thread; returns early when background_compilation is set or quitting.
	void WaitForShaderCompilation(std::string_view a_reason);
}
