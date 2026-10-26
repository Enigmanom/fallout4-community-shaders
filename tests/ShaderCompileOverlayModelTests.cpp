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

	// A blocked main menu without a visible window would look like a hang.
	void TestBlockingBatchIsVisible()
	{
		ShaderCompileOverlayModel model;
		const auto t0 = ShaderCompileOverlayModel::Clock::time_point{};
		const auto slowWarm = AllCacheHits(true, 3000ms);
		CHECK(cs::engine::BatchHoldsLoads(slowWarm));
		CHECK(model.Update(slowWarm, t0).phase == ShaderCompilePhase::kCompiling);
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
	TestBlockingBatchIsVisible();
	TestColdCacheShowsThenLingers();
	return cs::tests::failures ? 1 : 0;
}
