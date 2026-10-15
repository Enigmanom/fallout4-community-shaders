#include "Render/FrameBoundary.h"

#include <atomic>
#include <exception>
#include <mutex>
#include <utility>
#include <vector>

#include "Log.h"
#include "Render/EngineCallSite.h"
#include "Render/FrameDispatcher.h"

namespace cs::render
{
	namespace
	{
		auto* L = cs::log::Get("cs.render.frameboundary");

		constexpr engine::CallSiteAnchor kOnIdleSwapCall{
			.name = "Main::OnIdle -> Main::Swap",
			.function = RE::ID::Main::OnIdle,
			.offset = { 0x6EC, 0xCDC, 0xCDC },
			.target = RE::ID::Main::Swap
		};

		std::mutex registrationMutex;
		std::atomic<bool> installed{ false };
		std::atomic<bool> attempted{ false };
		std::vector<FrameBoundaryCallback> preSwap;
		std::vector<FrameBoundaryCallback> postSwap;

		bool Register(std::vector<FrameBoundaryCallback>& a_list, FrameBoundaryCallback a_callback)
		{
			std::scoped_lock lock(registrationMutex);
			if (!a_callback || attempted.load(std::memory_order_acquire))
				return false;
			a_list.push_back(std::move(a_callback));
			return true;
		}

		// A throwing callback must never unwind into the engine's main loop.
		void RunAll(const std::vector<FrameBoundaryCallback>& a_list, const char* a_phase) noexcept
		{
			for (const auto& callback : a_list) {
				try {
					callback();
				} catch (const std::exception& e) {
					L->error("Frame boundary {} callback threw: {}", a_phase, e.what());
				} catch (...) {
					L->error("Frame boundary {} callback threw", a_phase);
				}
			}
		}

		struct OnIdle_Swap
		{
			static void thunk(RE::Main* a_main)
			{
				RunAll(preSwap, "pre-swap");
				func(a_main);
				// Same thread as DearModdingUI page callbacks (they run inside Present).
				for (const auto& failure : GetFrameDispatcher().Drain())
					L->error("Frame task failed: {}", failure);
				RunAll(postSwap, "post-swap");
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	bool RegisterPreSwap(FrameBoundaryCallback a_callback)
	{
		return Register(preSwap, std::move(a_callback));
	}

	bool RegisterPostSwap(FrameBoundaryCallback a_callback)
	{
		return Register(postSwap, std::move(a_callback));
	}

	bool InstallFrameBoundary()
	{
		std::scoped_lock lock(registrationMutex);
		if (attempted.exchange(true, std::memory_order_acq_rel))
			return installed.load(std::memory_order_acquire);
		try {
			const auto swap = engine::ResolveCallSite(kOnIdleSwapCall);
			if (!swap) {
				L->error("Frame boundary unavailable; anchor '{}' failed: {}",
					kOnIdleSwapCall.name, swap.error());
				return false;
			}
			stl::write_thunk_call<OnIdle_Swap>(*swap);
			installed.store(true, std::memory_order_release);
			L->info("Installed frame boundary hook at '{}'", kOnIdleSwapCall.name);
			return true;
		} catch (const std::exception& e) {
			L->error("Frame boundary anchor '{}' install threw: {}",
				kOnIdleSwapCall.name, e.what());
		} catch (...) {
			L->error("Frame boundary anchor '{}' install threw", kOnIdleSwapCall.name);
		}
		return false;
	}

	bool FrameBoundaryInstalled() noexcept
	{
		return installed.load(std::memory_order_acquire);
	}
}
