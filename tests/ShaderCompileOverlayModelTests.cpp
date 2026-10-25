#include "Host/ShaderCompileOverlayModel.h"

#include "Check.h"

#include <iostream>
#include <string_view>

namespace
{
	using namespace std::chrono_literals;
	using cs::engine::ShaderCompileBatch;
	using cs::host::ShaderCompileOverlayModel;
	using cs::host::ShaderCompilePhase;

	ShaderCompileBatch Batch(bool a_active, std::chrono::milliseconds a_duration)
	{
		return { .completedTasks = 90, .failedTasks = 10, .totalTasks = 100, .cacheHits = 40, .duration = a_duration, .active = a_active };
	}

	ShaderCompileBatch AllCacheHits(bool a_active, std::chrono::milliseconds a_duration)
	{
		return { .completedTasks = 100, .totalTasks = 100, .cacheHits = 100, .duration = a_duration, .active = a_active };
	}

	void TestWarmCacheNeverShows()
	{
		ShaderCompileOverlayModel model;
		const auto t0 = ShaderCompileOverlayModel::Clock::time_point{};
		CHECK(model.Update(Batch(true, 200ms), t0).phase == ShaderCompilePhase::kHidden);
		CHECK(model.Update(Batch(false, 800ms), t0 + 800ms).phase == ShaderCompilePhase::kHidden);
		CHECK(model.Update(Batch(false, 800ms), t0 + 900ms).phase == ShaderCompilePhase::kHidden);
	}

	void TestSlowWarmCacheNeverShows()
	{
		ShaderCompileOverlayModel model;
		const auto t0 = ShaderCompileOverlayModel::Clock::time_point{};
		CHECK(model.Update(AllCacheHits(true, 3000ms), t0).phase == ShaderCompilePhase::kHidden);
		CHECK(model.Update(AllCacheHits(false, 3000ms), t0 + 10ms).phase == ShaderCompilePhase::kHidden);
	}

	void TestColdCacheShowsThenLingers()
	{
		ShaderCompileOverlayModel model;
		const auto t0 = ShaderCompileOverlayModel::Clock::time_point{};
		CHECK(model.Update(Batch(true, 1500ms), t0).phase == ShaderCompilePhase::kCompiling);
		// A dip below the reveal delay must not hide an already shown window.
		CHECK(model.Update(Batch(true, 100ms), t0 + 10ms).phase == ShaderCompilePhase::kCompiling);
		const auto done = model.Update(Batch(false, 45000ms), t0 + 20ms);
		CHECK(done.phase == ShaderCompilePhase::kReady);
		CHECK(done.batch.duration == 45000ms);
		// A later small batch must not replace or cancel the ready message.
		const auto during = model.Update(Batch(true, 50ms), t0 + 2000ms);
		CHECK(during.phase == ShaderCompilePhase::kReady);
		CHECK(during.batch.duration == 45000ms);
		CHECK(model.Update(Batch(false, 60ms), t0 + 20ms + ShaderCompileOverlayModel::kReadyLinger).phase == ShaderCompilePhase::kHidden);
	}
}

int main()
{
	TestWarmCacheNeverShows();
	TestSlowWarmCacheNeverShows();
	TestColdCacheShowsThenLingers();
	return cs::tests::failures ? 1 : 0;
}
