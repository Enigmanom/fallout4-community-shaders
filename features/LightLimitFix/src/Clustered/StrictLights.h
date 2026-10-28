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
	// Lights bound to portal rooms, and the per-frame room numbering they share with draws.
	class StrictLights
	{
	public:
		// Bits in LightData::roomFlags.
		static constexpr std::uint32_t kMaxRooms = 128;

		static bool IsGlobalLight(const RE::BSLight& a_light) noexcept;

		void Clear() noexcept { _rooms.clear(); }
		// Flags a non-global light and sets a bit for each room and portal it reaches.
		bool Mark(const RE::BSLight& a_light, LightData& a_data);
		// -1 when the pass is not in a numbered room.
		std::int32_t RoomIndex(const RE::BSRenderPass& a_pass) const noexcept;

	private:
		void AddRoom(const RE::NiNode* a_node, LightData& a_data);

		std::unordered_map<const RE::NiNode*, std::uint8_t> _rooms;
	};

	// b3 for forward effect and water draws; uploads only when the header changes.
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
