#pragma once

#include "LightData.h"
#include "Utils/CSBuffer.h"

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace RE
{
	class BSLight;
	class BSRenderPass;
	class NiNode;
}

namespace cs::features::light_limit_fix
{
	class StrictLights
	{
	public:
		static constexpr std::uint32_t kMaxRooms = 128;

		static bool IsGlobalLight(const RE::BSLight& a_light) noexcept;

		void Clear() noexcept { _rooms.clear(); }
		bool Mark(const RE::BSLight& a_light, LightData& a_data);
		std::int32_t RoomIndex(const RE::BSRenderPass& a_pass) const noexcept;

	private:
		void AddRoom(const RE::NiNode* a_node, LightData& a_data);

		std::unordered_map<const RE::NiNode*, std::uint8_t> _rooms;
	};

	class StrictLightBuffer
	{
	public:
		void Create();
		void Update(const StrictLightDataCB& a_next);
		ID3D11Buffer* Buffer() const { return _buffer ? _buffer->CB() : nullptr; }

	private:
		std::unique_ptr<buffer::ConstantBuffer> _buffer;
		StrictLightDataCB _uploaded;
		bool _hasUploaded = false;
	};
}
