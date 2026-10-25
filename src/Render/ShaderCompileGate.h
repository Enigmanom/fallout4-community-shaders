#pragma once

namespace cs::engine
{
	// Logs one summary line per finished compile batch; call once at startup.
	void InstallShaderCompileSummaryLog();

	// False when background_compilation lets play start while shaders compile.
	[[nodiscard]] bool ShaderCompileBlocksPlay();

	// Frame thread; cancels pending compiles once the game is quitting.
	void StopShaderCompileOnQuit() noexcept;
}
