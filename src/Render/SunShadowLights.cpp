#include "Render/SunShadowLights.h"

#include "Log.h"
#include "Render/Annotation.h"
#include "Render/DirectionalShadowLightData.h"
#include "Render/Engine.h"
#include "Render/RenderHooks.h"

#include <d3d11.h>
#include <winrt/base.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <format>

namespace cs::engine
{
	namespace
	{
		auto* L = cs::log::Get("cs.render.sunshadowlights");

		struct Slot
		{
			const SunShadowLightLayout* layout = nullptr;
			winrt::com_ptr<ID3D11Buffer> buffer;
			winrt::com_ptr<ID3D11ShaderResourceView> srv;
		};

		constexpr std::size_t kMaxLayoutCascades = 4;
		// Large enough for every layout.
		struct alignas(16) Scratch
		{
			std::byte bytes[sizeof(DirectionalShadowLightData<kMaxLayoutCascades>)];
		};

		bool g_hooked = false;
		SunShadowLightState g_state;
		// Indexed by the layout's cascade count.
		std::array<Slot, kMaxLayoutCascades + 1> g_slots;

		[[nodiscard]] bool EnsureResources(Slot& a_slot)
		{
			if (a_slot.srv)
				return true;
			auto* device = GetDevice();
			if (!device)
				return false;
			const auto stride = static_cast<UINT>(a_slot.layout->stride);
			const D3D11_BUFFER_DESC desc{
				.ByteWidth = stride,
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_SHADER_RESOURCE,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
				.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
				.StructureByteStride = stride
			};
			const Scratch zeroed{};
			const D3D11_SUBRESOURCE_DATA initial{ .pSysMem = &zeroed };
			const D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{
				.Format = DXGI_FORMAT_UNKNOWN,
				.ViewDimension = D3D11_SRV_DIMENSION_BUFFER,
				.Buffer = { .FirstElement = 0, .NumElements = 1 }
			};
			winrt::com_ptr<ID3D11Buffer> buffer;
			winrt::com_ptr<ID3D11ShaderResourceView> srv;
			if (FAILED(device->CreateBuffer(&desc, &initial, buffer.put())) ||
				FAILED(device->CreateShaderResourceView(buffer.get(), &srvDesc, srv.put()))) {
				L->error("the {}-cascade light data buffer could not be created", a_slot.layout->cascades);
				return false;
			}
			const auto name = std::format("SunShadowLights{}", a_slot.layout->cascades);
			cs::render::annotation::SetName(buffer.get(), name);
			cs::render::annotation::SetName(srv.get(), name + ".SRV");
			a_slot.buffer = std::move(buffer);
			a_slot.srv = std::move(srv);
			return true;
		}

		[[nodiscard]] bool Write(ID3D11DeviceContext* a_context, const Slot& a_slot, const void* a_data)
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (!a_slot.buffer || FAILED(a_context->Map(a_slot.buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
				return false;
			std::memcpy(mapped.pData, a_data, a_slot.layout->stride);
			a_context->Unmap(a_slot.buffer.get(), 0);
			return true;
		}

		void Retire()
		{
			if (!g_state.published)
				return;
			g_state.published = false;
			g_state.snapshot = {};
			auto* context = GetImmediateContext();
			if (!context)
				return;
			const Scratch zeroed{};
			for (const auto& slot : g_slots) {
				if (slot.layout && !Write(context, slot, &zeroed))
					L->error("the {}-cascade light data buffer could not be zeroed", slot.layout->cascades);
			}
		}

		// Every acquired layout is built from one snapshot or none is published.
		[[nodiscard]] bool WriteAll(ID3D11DeviceContext* a_context, const SunCascadeSnapshot& a_snapshot)
		{
			if (!a_context)
				return false;
			std::array<Scratch, g_slots.size()> built;
			for (std::size_t i = 0; i < g_slots.size(); ++i) {
				if (g_slots[i].layout && !g_slots[i].layout->build(a_snapshot, &built[i]))
					return false;
			}
			for (std::size_t i = 0; i < g_slots.size(); ++i) {
				if (g_slots[i].layout && !(EnsureResources(g_slots[i]) && Write(a_context, g_slots[i], &built[i])))
					return false;
			}
			return true;
		}

		void Publish()
		{
			SunCascadeSnapshot snapshot;
			auto status = TryGetSunCascades(snapshot);
			if (status == SunCascadeStatus::kOk && !WriteAll(GetImmediateContext(), snapshot))
				status = SunCascadeStatus::kInvalid;
			if (status != SunCascadeStatus::kOk) {
				// A failed frame must not leave the previous cascades visible.
				Retire();
				g_state.status = status;
				return;
			}
			g_state.status = status;
			g_state.published = true;
			++g_state.generation;
			g_state.snapshot = snapshot;
		}
	}

	bool AcquireSunShadowLights(const SunShadowLightLayout& a_layout)
	{
		if (!g_hooked) {
			// Early: feature callbacks on the same hook read the published state.
			if (!RegisterPostSunShadowRender(Publish, HookPriority::Early))
				return false;
			RegisterPreDeferredLightsImpl(Retire, HookPriority::Early);
			g_hooked = true;
		}
		g_slots[a_layout.cascades].layout = &a_layout;
		return true;
	}

	const SunShadowLightState& GetSunShadowLightState() noexcept
	{
		return g_state;
	}

	ID3D11ShaderResourceView* GetSunShadowLightsSRV(const SunShadowLightLayout& a_layout) noexcept
	{
		auto& slot = g_slots[a_layout.cascades];
		return slot.layout && EnsureResources(slot) ? slot.srv.get() : nullptr;
	}
}
