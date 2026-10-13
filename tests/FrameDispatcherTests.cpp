#include "Render/FrameDispatcher.h"

#include <atomic>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace
{
	using namespace std::chrono_literals;
	using cs::render::FrameDispatcher;

	int failures{};

	void Check(bool a_condition, std::string_view a_expression, int a_line)
	{
		if (!a_condition) {
			std::cerr << "CHECK failed at line " << a_line << ": " << a_expression << '\n';
			++failures;
		}
	}

#define CHECK(a_expression) Check(static_cast<bool>(a_expression), #a_expression, __LINE__)

	void TestRunsOnDrainingThread()
	{
		FrameDispatcher dispatcher;
		std::atomic_bool stop{ false };
		std::thread frames{ [&] {
			while (!stop.load()) {
				(void)dispatcher.Drain();
				std::this_thread::sleep_for(1ms);
			}
		} };
		const auto frameThread = frames.get_id();
		const auto result = dispatcher.RunOnFrame<bool>([&] { return std::this_thread::get_id() == frameThread; });
		stop = true;
		frames.join();
		CHECK(result && *result);
	}

	void TestTimeoutCancelsLateTask()
	{
		FrameDispatcher dispatcher;
		bool ran = false;
		const auto result = dispatcher.RunOnFrame<void>([&] { ran = true; }, 20ms);
		CHECK(!result);
		CHECK(result.error().starts_with("frame thread did not run within 20ms"));
		CHECK(dispatcher.Drain().empty());
		CHECK(!ran);
	}

	void TestExceptionsReturnAsErrors()
	{
		FrameDispatcher dispatcher;
		std::atomic_bool stop{ false };
		std::thread frames{ [&] {
			while (!stop.load()) {
				(void)dispatcher.Drain();
				std::this_thread::sleep_for(1ms);
			}
		} };
		const auto result = dispatcher.RunOnFrame<int>([]() -> int { throw std::runtime_error("boom"); });
		stop = true;
		frames.join();
		CHECK(!result);
		CHECK(result.error() == "boom");
	}

	void TestPostFailuresAndBackpressure()
	{
		FrameDispatcher dispatcher;
		CHECK(dispatcher.Post([] { throw std::runtime_error("posted"); }));
		const auto failed = dispatcher.Drain();
		CHECK(failed.size() == 1 && failed.front() == "posted");

		for (std::size_t i = 0; i < FrameDispatcher::kMaxQueued; ++i)
			CHECK(dispatcher.Post([] {}));
		CHECK(!dispatcher.Post([] {}));
		const auto rejected = dispatcher.RunOnFrame<void>([] {}, 20ms);
		CHECK(!rejected);
	}
}

int main()
{
	TestRunsOnDrainingThread();
	TestTimeoutCancelsLateTask();
	TestExceptionsReturnAsErrors();
	TestPostFailuresAndBackpressure();
	if (failures)
		return 1;
	std::cout << "Frame dispatcher tests passed\n";
	return 0;
}
