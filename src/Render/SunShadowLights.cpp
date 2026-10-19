#include "Render/SunShadowLights.h"

#include "Log.h"
#include "Render/Annotation.h"
#include "Render/DirectionalShadowLightData.h"
#include "Render/Engine.h"
#include "Render/RenderHooks.h"

#include <d3d11.h>
#include <winrt/base.h>

#include <cstring>

namespace cs::engine
{
	namespace
	{
		auto* L = cs::log::Get("cs.render.sunshadowlights");

		bool g_acquired = false;
		SunShadowLightState g_state;
		winrt::com_ptr<ID3D11Buffer> g_buffer;
		winrt::com_ptr<ID3D11ShaderResourceView> g_srv;

		[[nodiscard]] bool EnsureResources()
		{
			if (g_srv)
				return true;
			auto* device = GetDevice();
			if (!device)
				return false;
			const D3D11_BUFFER_DESC desc{
				.ByteWidth = sizeof(DirectionalShadowLightData),
				.Usage = D3D11_USAGE_DYNAMIC,
				.BindFlags = D3D11_BIND_SHADER_RESOURCE,
				.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
				.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED,
				.StructureByteStride = sizeof(DirectionalShadowLightData)
			};
			const DirectionalShadowLightData zeroed{};
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
				L->error("the light data buffer could not be created");
				return false;
			}
			cs::render::annotation::SetName(buffer.get(), "SunShadowLights");
			cs::render::annotation::SetName(srv.get(), "SunShadowLights.SRV");
			g_buffer = std::move(buffer);
			g_srv = std::move(srv);
			return true;
		}

		[[nodiscard]] bool Write(ID3D11DeviceContext* a_context, const DirectionalShadowLightData& a_data)
		{
			D3D11_MAPPED_SUBRESOURCE mapped{};
			if (!g_buffer || FAILED(a_context->Map(g_buffer.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
				return false;
			std::memcpy(mapped.pData, &a_data, sizeof(a_data));
			a_context->Unmap(g_buffer.get(), 0);
			return true;
		}

		void Retire()
		{
			if (!g_state.published)
				return;
			g_state.published = false;
			g_state.snapshot = {};
			if (auto* context = GetImmediateContext(); context && !Write(context, {}))
				L->error("the light data buffer could not be zeroed");
		}

		void Publish()
		{
			SunCascadeSnapshot snapshot;
			auto status = TryGetSunCascades(snapshot);
			DirectionalShadowLightData data;
			if (status == SunCascadeStatus::kOk && !BuildDirectionalShadowLightData(snapshot, data))
				status = SunCascadeStatus::kInvalid;
			auto* context = GetImmediateContext();
			if (status == SunCascadeStatus::kOk && !(context && EnsureResources() && Write(context, data)))
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

	bool AcquireSunShadowLights()
	{
		if (g_acquired)
			return true;
		// Early: feature callbacks on the same hook read the published state.
		if (!RegisterPostSunShadowRender(Publish, HookPriority::Early))
			return false;
		RegisterPreDeferredLightsImpl(Retire, HookPriority::Early);
		g_acquired = true;
		return true;
	}

	const SunShadowLightState& GetSunShadowLightState() noexcept
	{
		return g_state;
	}

	ID3D11ShaderResourceView* GetSunShadowLightsSRV() noexcept
	{
		return EnsureResources() ? g_srv.get() : nullptr;
	}
}
