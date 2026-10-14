#pragma once

#include <optional>

namespace cs::engine
{
	inline constexpr float kVanillaTimeScale = 20.0f;

	// Keeps time running while menus that hang on a zero timescale are open.
	void InstallGameTimeMenuGuard();

	void PauseGameTime();
	void ResumeGameTime();
	bool IsGameTimePaused() noexcept;
	// The scale restored on resume while paused.
	std::optional<float> GetTimeScale();
	void SetTimeScale(float a_scale);
	void ResetTimeScale();

	std::optional<float> GetGameHour();
	void SetGameHour(float a_hour);
}
