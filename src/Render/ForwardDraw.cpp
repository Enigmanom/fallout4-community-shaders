#include "Render/ForwardDraw.h"

#include "Feature.h"
#include "PCH.h"
#include "Render/Engine.h"
#include "Render/RuntimeCallback.h"
#include "Render/ShaderGeometryHooks.h"
#include "Render/ShaderInjection.h"
#include "Render/SubstrateSlots.h"
#include "Utils/CSBuffer.h"

#include "RE/B/BSRenderPass.h"
#include "RE/B/BSShaderAccumulator.h"
#include "RE/B/BSShaderManager.h"

#include <memory>
#include <vector>

namespace cs::render::forward_draw
{
	namespace
	{
		struct Subscription
		{
			Feature* owner;
			std::string_view label;
			Observer observer;
		};

		// 16 bytes, matching cbuffer PerShader in FO4\Permutation.hlsli.
		struct alignas(16) Descriptor
		{
			std::uint32_t extraShaderDescriptor = 0;
			std::uint32_t pad[3]{};
		};
		static_assert(sizeof(Descriptor) == 16);

		std::vector<Subscription> g_subscriptions;
		thread_local Draw t_draw;
		thread_local bool t_active = false;

		std::unique_ptr<buffer::ConstantBuffer> g_buffer;
		Descriptor g_uploaded;
		bool g_hasUploaded = false;

		// MainRenderSetup stores 13 instead of 24 on the world accumulators in some modes.
		[[nodiscard]] bool IsWorldAccumulator(const RE::BSShaderAccumulator* a_accumulator) noexcept
		{
			using Mode = RE::BSShaderManager::etRenderMode;
			if (!a_accumulator)
				return false;
			const auto mode = a_accumulator->renderMode;
			if (mode != Mode::kDeferredGBuffer && mode != Mode::kDepthPrepass)
				return false;
			const auto* scene = a_accumulator->activeShadowSceneNode;
			return scene && scene == engine::GetWorldShadowSceneNode();
		}

		void SetupGeometry(RE::BSShader*, RE::BSRenderPass* a_pass) noexcept
		{
			auto* accumulator = RE::BSShaderManager::GetCurrentAccumulator();
			t_draw = { a_pass, accumulator, IsWorldAccumulator(accumulator) };
			t_active = true;
			for (const auto& subscription : g_subscriptions)
				RunRenderCallback(subscription.owner, subscription.label, [&] { subscription.observer(t_draw); });
		}

		void RestoreGeometry(RE::BSShader*, RE::BSRenderPass*) noexcept
		{
			t_active = false;
			t_draw = {};
		}
	}

	const Draw* Current() noexcept
	{
		return t_active ? &t_draw : nullptr;
	}

	void Subscribe(Feature* a_owner, std::string_view a_label, Observer a_observer)
	{
		g_subscriptions.push_back({ a_owner, a_label, a_observer });
	}

	void Install()
	{
		for (const auto target : { engine::ShaderInjectionTarget::kEffect, engine::ShaderInjectionTarget::kBsWater })
			engine::ObserveShaderGeometry(target, { &SetupGeometry, &RestoreGeometry });
	}

	void BindDescriptor(ID3D11DeviceContext* a_context) noexcept
	{
		ID3D11Buffer* bound = nullptr;
		RunRenderCallback(nullptr, "forward draw descriptor", [&] {
			Descriptor next;
			if (t_active && t_draw.inWorld)
				next.extraShaderDescriptor |= kInWorld;
			if (!g_buffer)
				g_buffer = std::make_unique<buffer::ConstantBuffer>(buffer::ConstantBufferDesc<Descriptor>());
			if (!g_hasUploaded || g_uploaded.extraShaderDescriptor != next.extraShaderDescriptor) {
				g_hasUploaded = false;
				g_buffer->Update(next);
				g_uploaded = next;
				g_hasUploaded = true;
			}
			bound = g_buffer->CB();
		});
		// A failed upload binds nothing, which reads as not in world.
		engine::BindInjectionConstantBuffers(a_context, kForwardDrawSlot, 1, &bound);
	}
}
