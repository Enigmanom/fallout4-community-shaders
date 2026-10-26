#pragma once

#include "Render/ShaderCompileGate.h"

#include <chrono>
#include <cstdint>
#include <optional>

namespace cs::host
{
	enum class ShaderCompilePhase : std::uint8_t
	{
		kHidden,
		kCompiling,
		kReady
	};

	struct ShaderCompilePresentation
	{
		ShaderCompilePhase phase{ ShaderCompilePhase::kHidden };
		engine::ShaderCompileBatch batch;
	};

	// Decides when the progress window shows; pure so it is testable.
	class ShaderCompileOverlayModel
	{
	public:
		using Clock = std::chrono::steady_clock;

		static constexpr std::chrono::milliseconds kReadyLinger{ 4000 };

		[[nodiscard]] ShaderCompilePresentation Update(
			const engine::ShaderCompileBatch& a_batch,
			Clock::time_point a_now) noexcept
		{
			if (a_batch.active) {
				_revealed = _revealed || engine::BatchHoldsLoads(a_batch);
				if (_revealed) {
					_readyUntil.reset();
					return { ShaderCompilePhase::kCompiling, a_batch };
				}
			} else if (_revealed) {
				_revealed = false;
				_readyUntil = a_now + kReadyLinger;
				_finished = a_batch;
			}
			if (_readyUntil && a_now < *_readyUntil)
				return { ShaderCompilePhase::kReady, _finished };
			_readyUntil.reset();
			return {};
		}

	private:
		bool _revealed{};
		std::optional<Clock::time_point> _readyUntil;
		engine::ShaderCompileBatch _finished;
	};
}
