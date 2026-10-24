#include "Render/RgbaPreview.h"

#include "Render/Annotation.h"
#include "Utils/CSUtil.h"

#include <format>

namespace cs::render
{
	void RgbaPreview::Create(ID3D11Device* a_device, std::uint32_t a_width, std::uint32_t a_height, std::string_view a_name)
	{
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = a_width;
		desc.Height = a_height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		DX::ThrowIfFailed(a_device->CreateTexture2D(&desc, nullptr, _texture.put()));
		DX::ThrowIfFailed(a_device->CreateShaderResourceView(_texture.get(), nullptr, _srv.put()));
		DX::ThrowIfFailed(a_device->CreateUnorderedAccessView(_texture.get(), nullptr, _uav.put()));
		annotation::SetName(_texture.get(), std::format("{}.Texture", a_name));
		annotation::SetName(_srv.get(), std::format("{}.SRV", a_name));
		annotation::SetName(_uav.get(), std::format("{}.UAV", a_name));
		_width = a_width;
		_height = a_height;
		Invalidate();
	}

	void RgbaPreview::Convert(ID3D11DeviceContext* a_context, ID3D11ComputeShader* a_shader, ID3D11ShaderResourceView* a_source, ID3D11SamplerState* a_sampler)
	{
		if (!a_context || !a_shader || !a_source || !_uav)
			return;
		ID3D11UnorderedAccessView* uav = _uav.get();
		ID3D11UnorderedAccessView* const noUav = nullptr;
		ID3D11ShaderResourceView* const noSrv = nullptr;
		ID3D11SamplerState* const noSampler = nullptr;
		a_context->CSSetShaderResources(0, 1, &a_source);
		a_context->CSSetUnorderedAccessViews(0, 1, &uav, nullptr);
		if (a_sampler)
			a_context->CSSetSamplers(0, 1, &a_sampler);
		a_context->CSSetShader(a_shader, nullptr, 0);
		a_context->Dispatch((_width + 7) / 8, (_height + 7) / 8, 1);
		a_context->CSSetShaderResources(0, 1, &noSrv);
		a_context->CSSetUnorderedAccessViews(0, 1, &noUav, nullptr);
		if (a_sampler)
			a_context->CSSetSamplers(0, 1, &noSampler);
		a_context->CSSetShader(nullptr, nullptr, 0);
		_populated.store(true, std::memory_order_release);
	}

	FeatureDebugTexture RgbaPreview::GetDebugTexture(std::string_view a_unavailableText) const noexcept
	{
		FeatureDebugTexture texture{ .unavailableText = a_unavailableText };
		if (!_populated.load(std::memory_order_acquire) || !_srv)
			return texture;
		texture.texture = _srv.get();
		texture.width = _width;
		texture.height = _height;
		return texture;
	}
}
