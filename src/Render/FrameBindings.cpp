#include "Render/FrameBindings.h"

#include <d3d11.h>

namespace cs::engine
{
	namespace
	{
		// Calls the VS, PS or CS form of a context method; other stages no-op.
		template <auto VS, auto PS, auto CS, class... Args>
		void ForStage(ID3D11DeviceContext* a_context, ShaderStage a_stage, Args... a_args) noexcept
		{
			switch (a_stage) {
			case ShaderStage::kVertex:
				(a_context->*VS)(a_args...);
				break;
			case ShaderStage::kPixel:
				(a_context->*PS)(a_args...);
				break;
			case ShaderStage::kCompute:
				(a_context->*CS)(a_args...);
				break;
			default:
				break;
			}
		}

		template <class T, std::size_t Count>
		struct Bindings
		{
			std::array<T*, Count> expected{};
			std::bitset<Count> tracked;
			// Still bound as recorded since the last invalidation.
			std::bitset<Count> trusted;
			// Bound once per pass, so any change before the next pass bind is a loss.
			std::bitset<Count> persistent;

			void Record(UINT a_start, UINT a_count, T* const* a_values, bool a_persistent) noexcept
			{
				for (UINT i = 0; i < a_count; ++i) {
					expected[a_start + i] = a_values[i];
					tracked.set(a_start + i);
					trusted.set(a_start + i);
					persistent.set(a_start + i, a_persistent);
				}
			}

			UINT Bind(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, T* const* a_values, FrameBindingMetrics& a_metrics) noexcept
			{
				UINT calls = 0;
				for (UINT i = 0; i < a_count;) {
					const UINT slot = a_start + i;
					if (trusted.test(slot) && expected[slot] == a_values[i]) {
						++a_metrics.skipped;
						++i;
						continue;
					}
					UINT run = 1;
					while (i + run < a_count && !(trusted.test(slot + run) && expected[slot + run] == a_values[i + run]))
						++run;
					SetStageBindings(a_context, a_stage, slot, run, a_values + i);
					Record(slot, run, a_values + i, false);
					++calls;
					i += run;
				}
				return calls;
			}

			void Verify(ID3D11DeviceContext* a_context, ShaderStage a_stage, std::bitset<Count>& a_lost, FrameBindingMetrics& a_metrics) noexcept
			{
				for (UINT slot = 0; slot < Count; ++slot) {
					if (!tracked.test(slot))
						continue;
					T* actual = nullptr;
					GetStageBindings(a_context, a_stage, slot, 1, &actual);
					++a_metrics.checks;
					// Untrusted draw bindings may change legitimately; they rebind on next use.
					if (actual != expected[slot] && (trusted.test(slot) || persistent.test(slot))) {
						++a_metrics.lost;
						++a_metrics.lostTotal;
						a_lost.set(slot);
						trusted.reset(slot);
					}
					if (actual)
						actual->Release();
				}
			}
		};

		struct StageBindings
		{
			Bindings<ID3D11ShaderResourceView, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> resources;
			Bindings<ID3D11Buffer, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT> buffers;
			std::bitset<static_cast<std::size_t>(ShaderInjectionTarget::kCount)> sampled;
		};

		thread_local std::array<StageBindings, 3> t_bindings;
		thread_local FrameBindingMetrics t_metrics;
		thread_local std::uint64_t t_epoch = 0;

		StageBindings* Stage(ShaderStage a_stage) noexcept
		{
			const auto index = static_cast<std::size_t>(a_stage);
			return index < t_bindings.size() ? &t_bindings[index] : nullptr;
		}
	}

	void SetStageBindings(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11ShaderResourceView* const* a_values) noexcept
	{
		ForStage<&ID3D11DeviceContext::VSSetShaderResources, &ID3D11DeviceContext::PSSetShaderResources, &ID3D11DeviceContext::CSSetShaderResources>(a_context, a_stage, a_start, a_count, a_values);
	}

	void SetStageBindings(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11Buffer* const* a_values) noexcept
	{
		ForStage<&ID3D11DeviceContext::VSSetConstantBuffers, &ID3D11DeviceContext::PSSetConstantBuffers, &ID3D11DeviceContext::CSSetConstantBuffers>(a_context, a_stage, a_start, a_count, a_values);
	}

	void SetStageBindings(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11SamplerState* const* a_values) noexcept
	{
		ForStage<&ID3D11DeviceContext::VSSetSamplers, &ID3D11DeviceContext::PSSetSamplers, &ID3D11DeviceContext::CSSetSamplers>(a_context, a_stage, a_start, a_count, a_values);
	}

	void GetStageBindings(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11ShaderResourceView** a_values) noexcept
	{
		ForStage<&ID3D11DeviceContext::VSGetShaderResources, &ID3D11DeviceContext::PSGetShaderResources, &ID3D11DeviceContext::CSGetShaderResources>(a_context, a_stage, a_start, a_count, a_values);
	}

	void GetStageBindings(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11Buffer** a_values) noexcept
	{
		ForStage<&ID3D11DeviceContext::VSGetConstantBuffers, &ID3D11DeviceContext::PSGetConstantBuffers, &ID3D11DeviceContext::CSGetConstantBuffers>(a_context, a_stage, a_start, a_count, a_values);
	}

	void GetStageBindings(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11SamplerState** a_values) noexcept
	{
		ForStage<&ID3D11DeviceContext::VSGetSamplers, &ID3D11DeviceContext::PSGetSamplers, &ID3D11DeviceContext::CSGetSamplers>(a_context, a_stage, a_start, a_count, a_values);
	}

	void ResetFrameBindings() noexcept
	{
		t_bindings = {};
		t_metrics = { .lostTotal = t_metrics.lostTotal };
		++t_epoch;
	}

	void InvalidateFrameBindings() noexcept
	{
		for (auto& stage : t_bindings) {
			stage.resources.trusted.reset();
			stage.buffers.trusted.reset();
		}
		++t_epoch;
	}

	std::uint64_t GetFrameBindingEpoch() noexcept { return t_epoch; }

	FrameBindingMetrics GetFrameBindingMetrics() noexcept { return t_metrics; }

	void BindFrameShaderResources(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11ShaderResourceView* const* a_views) noexcept
	{
		if (auto* stage = Stage(a_stage)) {
			SetStageBindings(a_context, a_stage, a_start, a_count, a_views);
			stage->resources.Record(a_start, a_count, a_views, true);
		}
	}

	void BindFrameConstantBuffers(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11Buffer* const* a_buffers) noexcept
	{
		if (auto* stage = Stage(a_stage)) {
			SetStageBindings(a_context, a_stage, a_start, a_count, a_buffers);
			stage->buffers.Record(a_start, a_count, a_buffers, true);
		}
	}

	std::uint32_t BindDrawShaderResources(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11ShaderResourceView* const* a_views) noexcept
	{
		auto* stage = Stage(a_stage);
		return stage ? stage->resources.Bind(a_context, a_stage, a_start, a_count, a_views, t_metrics) : 0;
	}

	std::uint32_t BindDrawConstantBuffers(ID3D11DeviceContext* a_context, ShaderStage a_stage, UINT a_start, UINT a_count, ID3D11Buffer* const* a_buffers) noexcept
	{
		auto* stage = Stage(a_stage);
		return stage ? stage->buffers.Bind(a_context, a_stage, a_start, a_count, a_buffers, t_metrics) : 0;
	}

	void VerifyFrameBindings(ID3D11DeviceContext* a_context, ShaderStage a_stage, ShaderInjectionTarget a_target) noexcept
	{
		auto* stage = Stage(a_stage);
		const auto target = static_cast<std::size_t>(a_target);
		if (!stage || stage->sampled.test(target))
			return;
		stage->sampled.set(target);
		const auto index = static_cast<std::size_t>(a_stage);
		stage->resources.Verify(a_context, a_stage, t_metrics.resources[index], t_metrics);
		stage->buffers.Verify(a_context, a_stage, t_metrics.buffers[index], t_metrics);
	}
}
