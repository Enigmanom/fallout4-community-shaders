#pragma once

#include "Render/ShaderInjectionTargets.h"

#include <array>
#include <bitset>
#include <cstdint>

struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;
struct ID3D11Buffer;
struct ID3D11SamplerState;

namespace cs::engine
{
	struct FrameBindingMetrics
	{
		std::uint64_t checks = 0;
		std::uint64_t lost = 0;
		std::uint64_t lostTotal = 0;
		std::uint64_t skipped = 0;
		std::array<std::bitset<128>, 3> resources;
		std::array<std::bitset<14>, 3> buffers;
	};

	// Stage-dispatched context binds and reads; unknown stages are no-ops.
	void SetStageBindings(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11ShaderResourceView* const*) noexcept;
	void SetStageBindings(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11Buffer* const*) noexcept;
	void SetStageBindings(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11SamplerState* const*) noexcept;
	void GetStageBindings(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11ShaderResourceView**) noexcept;
	void GetStageBindings(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11Buffer**) noexcept;
	void GetStageBindings(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11SamplerState**) noexcept;

	void ResetFrameBindings() noexcept;
	// Output or foreign GPU work may have unbound views; rebind on the next request.
	void InvalidateFrameBindings() noexcept;
	// Advances on every reset or invalidation; outputs bound before it may be gone.
	std::uint64_t GetFrameBindingEpoch() noexcept;
	FrameBindingMetrics GetFrameBindingMetrics() noexcept;
	void BindFrameShaderResources(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11ShaderResourceView* const*) noexcept;
	void BindFrameConstantBuffers(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11Buffer* const*) noexcept;
	// Binds only slots whose trusted binding differs; returns the D3D calls made.
	std::uint32_t BindDrawShaderResources(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11ShaderResourceView* const*) noexcept;
	std::uint32_t BindDrawConstantBuffers(ID3D11DeviceContext*, ShaderStage, std::uint32_t, std::uint32_t, ID3D11Buffer* const*) noexcept;
	// Once per frame per target: read back tracked slots and distrust any lost binding.
	void VerifyFrameBindings(ID3D11DeviceContext*, ShaderStage, ShaderInjectionTarget) noexcept;
}
