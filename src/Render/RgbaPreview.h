#pragma once

#include "DebugView.h"

#include <d3d11.h>

#include <atomic>
#include <cstdint>
#include <string_view>

#include <winrt/base.h>

namespace cs::render
{
	// RGBA8 copy so the menu can show source formats its importer rejects.
	class RgbaPreview
	{
	public:
		// Throws on device failure.
		void Create(ID3D11Device* a_device, std::uint32_t a_width, std::uint32_t a_height, std::string_view a_name);

		// Render thread. Marks the preview populated.
		void Convert(ID3D11DeviceContext* a_context, ID3D11ComputeShader* a_shader, ID3D11ShaderResourceView* a_source, ID3D11SamplerState* a_sampler = nullptr);
		void Invalidate() noexcept { _populated.store(false, std::memory_order_release); }

		// Empty until populated, so the menu never shows stale or unwritten texels.
		FeatureDebugTexture GetDebugTexture(std::string_view a_unavailableText) const noexcept;

	private:
		winrt::com_ptr<ID3D11Texture2D> _texture;
		winrt::com_ptr<ID3D11ShaderResourceView> _srv;
		winrt::com_ptr<ID3D11UnorderedAccessView> _uav;
		std::uint32_t _width = 0;
		std::uint32_t _height = 0;
		std::atomic_bool _populated{ false };
	};
}
