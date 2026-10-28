#pragma once

#include "Render/ShaderInjectionTargets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

struct ID3D11Device;
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

	// Size of cbuffer PerShader; the test compares it with the HLSL layout.
	inline constexpr std::size_t kDescriptorSize = 16;

	inline constexpr std::array kTargets{ engine::ShaderInjectionTarget::kEffect, engine::ShaderInjectionTarget::kBsWater };

	[[nodiscard]] constexpr bool Covers(engine::ShaderInjectionTarget a_target) noexcept
	{
		for (const auto target : kTargets) {
			if (target == a_target)
				return true;
		}
		return false;
	}

	struct Draw
	{
		RE::BSRenderPass* pass = nullptr;
		RE::BSShaderAccumulator* accumulator = nullptr;
		bool inWorld = false;
	};

	// Valid from SetupGeometry to RestoreGeometry; null otherwise.
	[[nodiscard]] const Draw* Current() noexcept;

	using Observer = void (*)(const Draw&);
	// Startup only; runs before the engine SetupGeometry, quarantined.
	void Subscribe(Feature* a_owner, std::string_view a_label, Observer a_observer);

	void Install();
	void Initialize(ID3D11Device* a_device);
	void BindDescriptor(ID3D11DeviceContext* a_context) noexcept;
}
