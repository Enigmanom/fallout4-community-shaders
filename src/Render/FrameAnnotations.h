#pragma once

#include <cstdint>

namespace RE
{
	class BSShader;
}

namespace cs::render::frame_annotations
{
	// Names engine passes and draws in GPU captures. The hooks exist only when
	// diagnostics.frame_annotations was on at startup; Enabled() gates the per-draw events.
	void Install();
	// Names the engine's render targets and depth stencils.
	void OnDataLoaded();

	[[nodiscard]] bool Installed() noexcept;
	// The saved setting; differs from Installed() until the next launch.
	[[nodiscard]] bool Requested() noexcept;
	// Saves the setting and, once installed, applies it to the live flag.
	bool SetRequested(bool a_requested);

	// Live flag for the per-draw events. Capture tools hold a ScopedForce to see them all.
	[[nodiscard]] bool Enabled() noexcept;

	class ScopedForce
	{
	public:
		ScopedForce() noexcept;
		~ScopedForce() noexcept;

		ScopedForce(const ScopedForce&) = delete;
		ScopedForce(ScopedForce&&) = delete;
		ScopedForce& operator=(const ScopedForce&) = delete;
		ScopedForce& operator=(ScopedForce&&) = delete;
	};

	// Called by the BeginTechnique hook after the native technique binds.
	void OnTechniqueBound(const RE::BSShader* a_shader, std::uint32_t a_pixelShaderId);
}
