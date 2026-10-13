#pragma once

#include "RE/B/BGSTerrainManager.h"
#include "RE/B/BSMultiBoundNode.h"
#include "RE/B/BSTArray.h"
#include "RE/N/NiNode.h"
#include "RE/T/TESFile.h"
#include "RE/T/TESWaterForm.h"
#include "RE/T/TESWorldSpace.h"

#include <cstdint>
#include <span>

namespace cs::engine::water
{
	namespace id
	{
		inline constexpr REL::VariantID Attach{ 1118335, 2213538, 2213538 };
		inline constexpr REL::VariantID Detach{ 824959, 2213540, 2213540 };
		inline constexpr REL::VariantID Visibility{ 529133, 2213664, 2213664 };
		inline constexpr REL::VariantID SetWorldSpace{ 1217659, 2192046, 2192046 };
		inline constexpr REL::VariantID ReleaseWorld{ 1484325, 2202930, 2202930 };
		inline constexpr REL::VariantID UpdateDisplacement{ 281128, 2213930, 2213930 };
	}
	struct WaterSystem;
	// FO4: these native terrain types are absent from the pinned CommonLibF4 API.
	struct TerrainNode
	{
		RE::BGSTerrainManager* manager;
		std::byte reserved[0x38];
		std::uint32_t flags;
		std::uint32_t index;
		std::int16_t baseCellX;
		std::int16_t baseCellY;

		std::uint32_t GetLODLevel() const noexcept { return (flags >> 21) & 0x3FC; }
	};
	static_assert(offsetof(TerrainNode, flags) == 0x40);
	static_assert(offsetof(TerrainNode, baseCellX) == 0x48);

	struct TerrainChunk
	{
		TerrainNode* node;
		RE::NiPointer<RE::BSMultiBoundNode> land;
		RE::NiPointer<RE::BSMultiBoundNode> water;
		RE::NiPointer<RE::NiNode> chunk;
		bool loaded;
		bool attached;
		bool waterAttached;
		bool prepared;
	};
	static_assert(offsetof(TerrainChunk, water) == 0x10);
	static_assert(offsetof(TerrainChunk, loaded) == 0x20);

	struct TileCell
	{
		const RE::NiAVObject* geometry;
		std::int32_t x;
		std::int32_t y;
	};

	RE::NiNode* GetWaterLOD();
	bool DisableVanillaWaterLOD();
	void Prepare(TerrainChunk* a_chunk);
	void RegisterVanillaWater(TerrainChunk* a_chunk);
	void ReserveWaterObjects(std::uint32_t a_count);
	bool InitializeTile(RE::NiAVObject* a_shape, RE::TESWaterForm* a_form, float a_height);
	void ConfigureTileProperty(RE::BSGeometry* a_shape);
	void RemoveWater(RE::NiAVObject* a_shape);
	void EnableWaterSystem();
	void UpdateVisibility(TerrainNode* a_node, RE::NiNode* a_parent, std::span<const TileCell> a_tiles);

	// FO4: read engine-owned cursors only at the DataLoaded boundary, before terrain streaming.
	bool ReadCellData(RE::TESWorldSpace* a_world, std::int32_t a_x, std::int32_t a_y,
		RE::TESFormID& a_form, float& a_water, float& a_land);
}
