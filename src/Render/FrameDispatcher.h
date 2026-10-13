#pragma once

#include <atomic>
#include <chrono>
#include <deque>
#include <expected>
#include <format>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cs::render
{
	// Runs work posted from any thread at the host frame boundary, where the host UI edits settings.
	class FrameDispatcher
	{
	public:
		static constexpr std::chrono::milliseconds kDefaultTimeout{ 5000 };
		static constexpr std::size_t kMaxQueued = 256;

		// Fire-and-forget; false when the queue is full because frames are not draining.
		[[nodiscard]] bool Post(std::function<void()> a_task);

		// Frame thread only. Returns one message per posted task that threw.
		[[nodiscard]] std::vector<std::string> Drain() noexcept;

		// Blocks the caller until the frame thread runs the task. A timeout cancels it, so it never
		// runs late; exceptions come back as errors.
		template <class T>
		[[nodiscard]] std::expected<T, std::string> RunOnFrame(
			std::function<T()> a_task,
			std::chrono::milliseconds a_timeout = kDefaultTimeout)
		{
			struct Shared
			{
				enum Phase : int
				{
					kQueued,
					kRunning,
					kCancelled
				};
				std::atomic<int> phase{ kQueued };
				std::promise<std::expected<T, std::string>> result;
			};
			auto shared = std::make_shared<Shared>();
			auto future = shared->result.get_future();
			const bool posted = Post([shared, task = std::move(a_task)] {
				int expected = Shared::kQueued;
				if (!shared->phase.compare_exchange_strong(expected, Shared::kRunning))
					return;
				try {
					if constexpr (std::is_void_v<T>) {
						task();
						shared->result.set_value({});
					} else {
						shared->result.set_value(task());
					}
				} catch (const std::exception& error) {
					shared->result.set_value(std::unexpected(std::string(error.what())));
				} catch (...) {
					shared->result.set_value(std::unexpected(std::string("non-standard exception")));
				}
			});
			if (!posted)
				return std::unexpected("frame queue is full; frames are not being presented");
			if (future.wait_for(a_timeout) != std::future_status::ready) {
				int expected = Shared::kQueued;
				if (shared->phase.compare_exchange_strong(expected, Shared::kCancelled)) {
					return std::unexpected(std::format(
						"frame thread did not run within {}ms", a_timeout.count()));
				}
				// The task already started; its side effects are real, so report its outcome.
			}
			return future.get();
		}

	private:
		std::mutex _mutex;
		std::deque<std::function<void()>> _queue;
	};

	FrameDispatcher& GetFrameDispatcher() noexcept;
}
