#pragma once

namespace RE
{
	class BSRenderPass;
}

namespace cs::engine
{
	// Decals and alpha-tested cards; shifting their UVs breaks them.
	[[nodiscard]] bool IsOverlayShape(RE::BSRenderPass* a_pass) noexcept;
}
