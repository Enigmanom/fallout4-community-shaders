#pragma once

namespace RE
{
	class TESWeather;
}

namespace cs::engine
{
	// Detours Sky::SetWeather and Sky::ForceWeather so a lock outranks every caller.
	void InstallWeatherLockHooks();
	bool AreWeatherLockHooksInstalled() noexcept;

	// Instant changes skip the transition; both retarget an active lock.
	void ApplyWeather(RE::TESWeather* a_weather, bool a_instant);
	// Returns to the climate's weather.
	void ResetWeather();

	void LockWeather(RE::TESWeather* a_weather);
	void UnlockWeather();
	RE::TESWeather* GetLockedWeather() noexcept;
	// Per frame: reclaims the sky when anything moved it off the locked weather.
	void MaintainWeatherLock();
}
