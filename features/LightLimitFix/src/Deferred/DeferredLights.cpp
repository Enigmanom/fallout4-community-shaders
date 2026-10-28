#include "DeferredLights.h"

#include <cstring>

namespace cs::features::light_limit_fix
{
	void DeferredLights::Create()
	{
		for (std::uint32_t side = 0; side < 2; ++side) {
			_tiled[side] = std::make_unique<buffer::StructuredBuffer>(
				buffer::StructuredBufferDesc<LightData>(kMaxLights, false, true), kMaxLights,
				side == 0 ? "LLF Tiled Lights 0" : "LLF Tiled Lights 1");
			_tiled[side]->CreateSRV();
		}
		_raster = std::make_unique<buffer::ConstantBuffer>(buffer::ConstantBufferDesc<LightData>());
		_raster->SetName("LLF Raster Light");
		_uploadedRaster.reset();
	}

	void DeferredLights::Record(std::uint32_t a_side, std::uint32_t a_index, const LightData& a_light)
	{
		std::scoped_lock lock(_mutex);
		_records[a_side][a_index] = a_light;
	}

	void DeferredLights::Clear()
	{
		std::scoped_lock lock(_mutex);
		_records = {};
	}

	bool DeferredLights::UploadTiled(std::uint32_t a_side, std::uint32_t a_count)
	{
		try {
			std::scoped_lock lock(_mutex);
			_tiled[a_side]->Update(_records[a_side].data(), a_count * sizeof(LightData));
		} catch (...) {
			return false;
		}
		_uploadedSide = a_side;
		return true;
	}

	ID3D11ShaderResourceView* DeferredLights::TiledView() const
	{
		return _tiled[_uploadedSide] ? _tiled[_uploadedSide]->SRV() : nullptr;
	}

	ID3D11Buffer* DeferredLights::RasterBuffer(const LightData& a_light)
	{
		if (!_uploadedRaster || std::memcmp(&*_uploadedRaster, &a_light, sizeof(LightData)) != 0) {
			try {
				_raster->Update(a_light);
			} catch (...) {
				_uploadedRaster.reset();
				return nullptr;
			}
			_uploadedRaster = a_light;
		}
		return _raster->CB();
	}
}
