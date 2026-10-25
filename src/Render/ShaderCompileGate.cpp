#include "Render/ShaderCompileGate.h"

#include "Log.h"
#include "Render/ShaderInjection.h"
#include "Render/ShaderVariantCompilation.h"
#include "Settings/FeatureConfig.h"

#include <RE/M/Main.h>

namespace cs::engine
{
	namespace
	{
		auto* L = cs::log::Get("cs.shadercache");

		// Smaller batches are incremental discoveries, not the startup compile.
		constexpr std::uint32_t kStartupBatchMinimum = 100;

		void LogSummary(const ShaderCompileBatch& a_batch)
		{
			const auto level = a_batch.totalTasks >= kStartupBatchMinimum ?
			                       spdlog::level::info :
			                       spdlog::level::debug;
			L->log(
				level,
				"Shader compilation finished: {} variants, {} compiled, {} cache hits, "
				"{} failed, {:.1f} s",
				a_batch.totalTasks,
				a_batch.CompiledTasks(),
				a_batch.cacheHits,
				a_batch.failedTasks,
				std::chrono::duration<double>(a_batch.duration).count());
		}
	}

	void InstallShaderCompileSummaryLog()
	{
		AddShaderCompileBatchObserver(&LogSummary);
	}

	bool ShaderCompileBlocksPlay()
	{
		// The setting is file-only, so one read serves the session.
		static const bool blocks = [] {
			std::string error;
			const bool background = feature_config::ParseBackgroundCompilation(feature_config::GetRoot(), error);
			if (!error.empty())
				L->warn("shader_cache.{}; using the default", error);
			return !background;
		}();
		return blocks;
	}

	void StopShaderCompileOnQuit() noexcept
	{
		const auto* main = RE::Main::GetSingleton();
		if (!main || !main->quitGame)
			return;
		static const bool stopped = [] {
			L->info("Game is quitting; stopping shader compilation");
			StopNativeShaderVariantCompilations();
			return true;
		}();
		(void)stopped;
	}
}
