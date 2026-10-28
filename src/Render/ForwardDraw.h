#pragma once

#include <cstdint>
#include <string_view>

struct ID3D11DeviceContext;

namespace RE
{
	class BSRenderPass;
	class BSShaderAccumulator;
}

namespace cs
{
	class Feature;
}

namespace cs::render::forward_draw
{
	// Mirrors Permutation::ExtraFlags in FO4\Permutation.hlsli.
	enum ExtraFlags : std::uint32_t
	{
		kInWorld = 1u << 0
	};

	struct Draw
	{
		RE::BSRenderPass* pass = nullptr;
		RE::BSShaderAccumulator* accumulator = nullptr;
		// World accumulator mode drawing into the world scene node.
		bool inWorld = false;
	};

	// Effect and water draws; valid from SetupGeometry until RestoreGeometry, else null.
	[[nodiscard]] const Draw* Current() noexcept;

	using Observer = void (*)(const Draw&);
	// Startup only; runs once per draw before the engine's SetupGeometry, under owner's quarantine.
	void Subscribe(Feature* a_owner, std::string_view a_label, Observer a_observer);

	void Install();
	// Binds the per-draw PS descriptor, uploading only when it changed.
	void BindDescriptor(ID3D11DeviceContext* a_context) noexcept;
}
