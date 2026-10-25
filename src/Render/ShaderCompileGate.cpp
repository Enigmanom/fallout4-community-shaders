#include "Render/ShaderCompileGate.h"

#include "Log.h"
#include "Render/ShaderVariantCompilation.h"
#include "Settings/FeatureConfig.h"

#include <RE/M/Main.h>

#include <mutex>
#include <optional>
#include <thread>

namespace cs::engine
{
	namespace
	{
		using namespace std::chrono_literals;
		using Clock = std::chrono::steady_clock;

		auto* L = cs::log::Get("cs.shadercache");

		constexpr auto kPollInterval = 100ms;
		constexpr auto kProgressLogInterval = 5s;
		// Smaller batches are incremental discoveries, not the startup compile.
		constexpr std::uint32_t kStartupBatchMinimum = 100;

		// Accrues gate wait so the batch summary can report it.
		class GateWaitClock
		{
		public:
			void Begin(Clock::time_point a_now)
			{
				const std::scoped_lock lock(_mutex);
				_since = a_now;
			}

			void End(Clock::time_point a_now)
			{
				const std::scoped_lock lock(_mutex);
				if (_since)
					_accrued += std::chrono::duration_cast<std::chrono::milliseconds>(a_now - *_since);
				_since.reset();
			}

			std::chrono::milliseconds Take(Clock::time_point a_now)
			{
				const std::scoped_lock lock(_mutex);
				auto total = _accrued;
				if (_since) {
					total += std::chrono::duration_cast<std::chrono::milliseconds>(a_now - *_since);
					_since = a_now;
				}
				_accrued = {};
				return total;
			}

		private:
			std::mutex _mutex;
			std::optional<Clock::time_point> _since;
			std::chrono::milliseconds _accrued{};
		};

		GateWaitClock& WaitClock()
		{
			static GateWaitClock clock;
			return clock;
		}

		bool Quitting() noexcept
		{
			const auto* main = RE::Main::GetSingleton();
			return main && main->quitGame;
		}

		void LogSummary(const ShaderCompileBatch& a_batch)
		{
			const auto waited = WaitClock().Take(Clock::now());
			const auto level = a_batch.totalTasks >= kStartupBatchMinimum ?
			                       spdlog::level::info :
			                       spdlog::level::debug;
			L->log(
				level,
				"Shader compilation finished: {} variants, {} compiled, {} cache hits, "
				"{} failed, {:.1f} s, gate wait {:.1f} s",
				a_batch.totalTasks,
				a_batch.CompiledTasks(),
				a_batch.cacheHits,
				a_batch.failedTasks,
				std::chrono::duration<double>(a_batch.duration).count(),
				std::chrono::duration<double>(waited).count());
		}
	}

	void InstallShaderCompileSummaryLog()
	{
		AddShaderCompileBatchObserver(&LogSummary);
	}

	void WaitForShaderCompilation(std::string_view a_reason)
	{
		if (!GetShaderCompileBatch().active)
			return;
		std::string settingError;
		const bool background = feature_config::ParseBackgroundCompilation(feature_config::GetRoot(), settingError);
		if (!settingError.empty())
			L->warn("shader_cache.{}; using the default", settingError);
		if (background) {
			L->info("Shader compilation still running before {}; background_compilation is on", a_reason);
			return;
		}

		const auto start = Clock::now();
		WaitClock().Begin(start);
		auto lastLog = start;
		const auto logProgress = [&](std::string_view a_state) {
			const auto batch = GetShaderCompileBatch();
			L->info(
				"{} shader compilation before {}: {}/{} done, {} failed",
				a_state,
				a_reason,
				batch.FinishedTasks(),
				batch.totalTasks,
				batch.failedTasks);
		};
		logProgress("Waiting for");
		bool quitting = false;
		while (GetShaderCompileBatch().active) {
			if (Quitting()) {
				quitting = true;
				break;
			}
			std::this_thread::sleep_for(kPollInterval);
			if (const auto now = Clock::now(); now - lastLog >= kProgressLogInterval) {
				lastLog = now;
				logProgress("Waiting for");
			}
		}
		WaitClock().End(Clock::now());
		if (quitting)
			L->info("Game is quitting; stopped waiting for shader compilation");
	}
}
