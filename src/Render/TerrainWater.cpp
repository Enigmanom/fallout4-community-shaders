#include "Render/TerrainWater.h"

#include "Log.h"
#include "RE/B/BSGeometry.h"
#include "RE/B/BSSpinLock.h"
#include "RE/N/NiUpdateData.h"
#include "RE/T/TES.h"
#include "RE/T/TESFileArray.h"
#include "RE/T/TESObjectCELL.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <mutex>

namespace cs::engine::water
{
	namespace
	{
		template <class T>
		T& Member(void* a_object, std::size_t a_offset)
		{
			return *reinterpret_cast<T*>(static_cast<std::byte*>(a_object) + a_offset);
		}

		using Holds = RE::BSTArray<RE::NiPointer<RE::NiRefObject>>;

		struct WaterObject : RE::NiRefObject
		{
			std::byte plane[0x10];
			RE::NiPointer<RE::NiAVObject> geometry;
			RE::NiPointer<RE::NiAVObject> ripple;
			RE::TESWaterForm* form;
			RE::NiPointer<RE::NiRefObject> reflections;
			RE::NiPointer<RE::NiRefObject> normals;
			RE::NiPointer<RE::NiRefObject> displacement;
		};
		using Objects = RE::BSTArray<RE::NiPointer<WaterObject>>;

		void* System()
		{
			static REL::Relocation<void**> system{ REL::VariantID{ 1006049, 2689094, 4796380 } };
			return *system;
		}

		bool AddWater(void* a_system, RE::NiAVObject* a_shape, RE::TESWaterForm* a_form, float a_height)
		{
			using Fn = bool (*)(void*, RE::NiAVObject*, RE::TESWaterForm*, float, void*, bool, bool);
			static REL::Relocation<Fn> fn{ REL::VariantID{ 1169411, 2213934, 2213934 } };
			return fn(a_system, a_shape, a_form, a_height, nullptr, true, false);
		}

		void QueueDeferredRelease(RE::NiPointer<RE::NiRefObject>& a_object)
		{
			// FO4: NiObjectGarbage::Add retains its argument; it does not consume it.
			static REL::Relocation<bool (*)(RE::NiPointer<RE::NiRefObject>&)> fn{ REL::VariantID{ 1093452, 2270153, 2270153 } };
			fn(a_object);
		}

		void RemoveOrphanedEntry(Holds& a_entries, const RE::NiRefObject* a_entry)
		{
			if (!a_entry || std::atomic_ref{ const_cast<std::uint32_t&>(a_entry->refCount) }.load() != 1)
				return;
			const auto it = std::ranges::find_if(a_entries, [&](const auto& candidate) { return candidate.get() == a_entry; });
			if (it == a_entries.end())
				return;
			RE::NiPointer<RE::NiRefObject> released{ it->get() };
			QueueDeferredRelease(released);
			a_entries.erase(it);
		}

		bool ActiveCell(RE::TESObjectCELL* a_cell)
		{
			static REL::Relocation<bool (*)(RE::TES*, RE::TESObjectCELL*, bool)> loaded{ REL::VariantID{ 1566182, 2192040, 2192040 } };
			static REL::Relocation<void**> visibility{ REL::VariantID{ 787073, 2689095, 4796381 } };
			static REL::Relocation<bool (*)(void*, std::uint32_t)> active{ REL::VariantID{ 708710, 2317378, 2317378 } };
			return a_cell && loaded(RE::TES::GetSingleton(), a_cell, true) && *visibility && active(*visibility, a_cell->GetFormID());
		}

		void EnableAll(RE::BSGeometrySegmentData* a_data)
		{
			static REL::Relocation<void (*)(RE::BSGeometrySegmentData*)> fn{ REL::VariantID{ 246125, 2270009, 2270009 } };
			fn(a_data);
		}

		void UpdateSegments(RE::BSGeometrySegmentData* a_data, const RE::BSScrapArray<RE::TESObjectCELL*>& a_cells)
		{
			static REL::Relocation<bool (*)(RE::BSGeometrySegmentData*, std::uint32_t, std::uint8_t)> enabled{ REL::VariantID{ 747791, 2270019, 2270019 } };
			static REL::Relocation<bool (*)(RE::BSGeometrySegmentData*, std::uint32_t, std::uint8_t)> empty{ REL::VariantID{ 654568, 2270020, 2270020 } };
			static REL::Relocation<void (*)(RE::BSGeometrySegmentData*, std::uint32_t, std::uint8_t, bool)> enable{ REL::VariantID{ 1134184, 2270017, 2270017 } };
			static REL::Relocation<void (*)(RE::BSGeometrySegmentData*, std::uint32_t, std::uint8_t, bool)> disable{ REL::VariantID{ 1466142, 2270018, 2270018 } };
			static REL::Relocation<void (*)(RE::BSGeometrySegmentData*)> refresh{ REL::VariantID{ 305668, 2270032, 2270032 } };
			const auto count = Member<std::uint8_t>(a_data, 0x3D) ? 1u : Member<std::uint32_t>(a_data, 0x2C);
			for (std::uint32_t i = 0; i < count; ++i) {
				const bool hide = !empty(a_data, i, 0xFF) && i < a_cells.size() && ActiveCell(a_cells[i]);
				if (enabled(a_data, i, 0xFF) == hide)
					(hide ? disable : enable)(a_data, i, 0xFF, false);
			}
			refresh(a_data);
		}

		bool SeekChunk(RE::TESFile* a_file, std::uint32_t a_type)
		{
			for (auto type = a_file->GetTESChunk(); type; type = a_file->GetTESChunk()) {
				if (type == a_type)
					return true;
				if (!a_file->NextChunk())
					break;
			}
			return false;
		}

		bool ReadChunk(RE::TESFile* a_file, void* a_data, std::uint32_t a_size)
		{
			// FO4: smaller capacities silently produce a NUL-terminated prefix, not a complete record.
			return a_file->actualChunkSize == a_size && a_file->GetChunkData(a_data, a_size);
		}
	}

	RE::NiNode* GetWaterLOD()
	{
		static REL::Relocation<RE::NiNode**> root{ REL::VariantID{ 1021890, 2698037, 2698037 } };
		return *root;
	}

	bool DisableVanillaWaterLOD()
	{
		// Skip iterating attached meshes and calling TESWaterSystem::AddLODWater, this is handled in Attach now
		static REL::Relocation<std::uintptr_t> attach{ REL::VariantID{ 1118335, 2213538, 2213538 } };
		static REL::Relocation<std::uintptr_t> registration{ REL::VariantID{ 748921, 2213558, 2213558 } };
		const std::array addresses{ attach.address() + 0x103, registration.address() + (REX::FModule::IsRuntimeOG() ? 0x67 : 0x10B) };
		const std::array<std::array<std::uint8_t, 6>, 2> expected = REX::FModule::IsRuntimeOG() ?
		                                                                std::array{ std::array<std::uint8_t, 6>{ 0x0F, 0x83, 0xC0, 0, 0, 0 }, std::array<std::uint8_t, 6>{ 0x0F, 0x83, 0x91, 0, 0, 0 } } :
		                                                                std::array{ std::array<std::uint8_t, 6>{ 0x0F, 0x83, 0xBC, 0, 0, 0 }, std::array<std::uint8_t, 6>{ 0x0F, 0x86, 0xA6, 0, 0, 0 } };
		for (std::size_t i = 0; i < addresses.size(); ++i)
			if (std::memcmp(reinterpret_cast<const void*>(addresses[i]), expected[i].data(), 6) != 0)
				return false;
		for (std::size_t i = 0; i < addresses.size(); ++i) {
			constexpr std::uint8_t patch[]{ 0x90, 0xE9 };
			if (!REL::WriteSafe(addresses[i], patch, sizeof(patch))) {
				for (std::size_t j = 0; j <= i; ++j)
					REL::WriteSafe(addresses[j], expected[j].data(), sizeof(patch));
				return false;
			}
		}
		return true;
	}

	void Prepare(TerrainChunk* a_chunk)
	{
		static REL::Relocation<void (*)(TerrainChunk*)> fn{ REL::VariantID{ 1339886, 2213552, 2213552 } };
		fn(a_chunk);
	}

	void RegisterVanillaWater(TerrainChunk* a_chunk)
	{
		static REL::Relocation<void (*)(void*, RE::NiAVObject*, RE::TESWorldSpace*, RE::NiNode*, RE::BSMultiBoundNode*, bool)> fn{ REL::VariantID{ 759998, 2213947, 2213947 } };
		const auto root = GetWaterLOD();
		if (!a_chunk->water || !root || !System())
			return;
		for (const auto& child : a_chunk->water->children) {
			if (!child)
				continue;
			if (auto geometry = child->IsGeometry(); geometry && geometry->GetSegmentData())
				EnableAll(geometry->GetSegmentData());
			fn(System(), child.get(), a_chunk->node->manager->world, root, a_chunk->water.get(), true);
		}
		RE::NiUpdateData update{};
		root->Update(update);
	}

	void ReserveWaterObjects(std::uint32_t a_count)
	{
		const auto system = System();
		if (!system)
			return;
		const std::scoped_lock guard(Member<RE::BSSpinLock>(system, 0x118));
		auto& objects = Member<Objects>(system, 0x18);
		objects.reserve(objects.size() + a_count);
	}

	bool InitializeTile(RE::NiAVObject* a_shape, RE::TESWaterForm* a_form, float a_height)
	{
		const auto system = System();
		if (!system || !a_shape || !a_form)
			return false;
		const std::scoped_lock guard(Member<RE::BSSpinLock>(system, 0x118));
		if (!AddWater(system, a_shape, a_form, a_height))
			return false;
		// Remove from WaterSystem, will manage it ourselves
		auto& objects = Member<Objects>(system, 0x18);
		if (objects.empty() || objects.back()->geometry.get() != a_shape) {
			EnableWaterSystem();
			RemoveWater(a_shape);
			return false;
		}
		const auto reflections = objects.back()->reflections.get();
		const auto normals = objects.back()->normals.get();
		objects.pop_back();
		RemoveOrphanedEntry(Member<Holds>(system, 0x30), reflections);
		RemoveOrphanedEntry(Member<Holds>(system, 0x48), normals);
		return true;
	}

	void ConfigureTileProperty(RE::BSGeometry* a_shape)
	{
		if (const auto property = a_shape->properties[1].get()) {
			// FO4: retain the native reflection path; its stock archive has no CUBEMAP techniques.
			auto& flags = Member<std::uint32_t>(property, 0x70);
			flags = (flags & ~0x2u) | 0xC4u;
		}
	}

	void RemoveWater(RE::NiAVObject* a_shape)
	{
		static REL::Relocation<bool (*)(void*, RE::NiAVObject*)> fn{ REL::VariantID{ 1284068, 2213937, 2213937 } };
		if (const auto system = System(); system && a_shape)
			fn(system, a_shape);
	}

	void EnableWaterSystem()
	{
		static REL::Relocation<void (*)(void*)> fn{ REL::VariantID{ 120736, 2213949, 2213949 } };
		if (const auto system = System())
			fn(system);
	}

	void UpdateVisibility(TerrainNode* a_node, RE::NiNode* a_parent, std::span<const TileCell> a_tiles)
	{
		if (!a_node || !a_parent)
			return;
		const auto tes = RE::TES::GetSingleton();
		const bool intersects = tes && !tes->interiorCell && (a_node->flags & 0x4000);
		RE::BSScrapArray<RE::TESObjectCELL*> cells;
		if (intersects) {
			static REL::Relocation<void (*)(TerrainNode*, RE::BSScrapArray<RE::TESObjectCELL*>&)> build{ REL::VariantID{ 1079398, 2213679, 2213679 } };
			build(a_node, cells);
		}
		for (const auto& child : a_parent->children) {
			if (!child)
				continue;
			if (auto geometry = child->IsGeometry(); geometry && geometry->GetSegmentData()) {
				if (intersects)
					UpdateSegments(geometry->GetSegmentData(), cells);
				else
					EnableAll(geometry->GetSegmentData());
				continue;
			}
			const auto tile = std::ranges::find_if(a_tiles, [&](const auto& item) { return item.geometry == child.get(); });
			if (tile == a_tiles.end())
				continue;
			const auto x = tile->x - a_node->baseCellX;
			const auto y = tile->y - a_node->baseCellY;
			const auto level = static_cast<std::int32_t>(a_node->GetLODLevel());
			// FO4: native BuildCellCache orders the borrowed cells x-major.
			const auto index = x * level + y;
			const bool hide = intersects && x >= 0 && y >= 0 && x < level && y < level &&
			                  index >= 0 && static_cast<std::size_t>(index) < cells.size() && ActiveCell(cells[static_cast<std::uint32_t>(index)]);
			child->SetAppCulled(hide);
		}
	}

	bool ReadCellData(RE::TESWorldSpace* a_world, std::int32_t a_x, std::int32_t a_y,
		RE::TESFormID& a_form, float& a_water, float& a_land)
	{
		static std::mutex cursorMutex;
		const std::scoped_lock lock(cursorMutex);
		static REL::Relocation<RE::TESFile* (*)(RE::TESFile*, std::uint32_t)> safe{ REL::VariantID{ 1287817, 2192556, 2192556 } };
		static REL::Relocation<bool (*)(RE::TESWorldSpace*, RE::TESFile*, std::int32_t, std::int32_t)> cell{ REL::VariantID{ 1012650, 2202847, 2202847 } };
		static REL::Relocation<bool (*)(RE::TESFile*)> land{ REL::VariantID{ 898709, 2200370, 2200370 } };
		static REL::Relocation<void (*)(RE::TESFormID&, RE::TESFile*)> resolve{ REL::VariantID{ 1315637, 2193152, 2193152 } };
		a_form = 0;
		a_water = FLT_MAX;
		a_land = FLT_MAX;
		const auto files = a_world->sourceFiles.array;
		if (!files || files->empty())
			return false;
		std::int32_t index = static_cast<std::int32_t>(files->size()) - 1;
		RE::TESFile* file = nullptr;
		for (; index >= 0; --index) {
			file = safe((*files)[index], 0x4000);
			if (file && cell(a_world, file, a_x, a_y))
				break;
		}
		if (index < 0)
			return false;
		constexpr auto xclw = 0x574C4358u;
		constexpr auto xcwt = 0x54574358u;
		if (SeekChunk(file, xclw) && !ReadChunk(file, &a_water, 4))
			return false;
		if (SeekChunk(file, xcwt)) {
			if (!ReadChunk(file, &a_form, 4))
				return false;
			if (a_form)
				resolve(a_form, file);
		}
		for (; index >= 0; --index) {
			file = safe((*files)[index], 0x4000);
			if (file && cell(a_world, file, a_x, a_y) && land(file))
				break;
		}
		if (index < 0)
			return false;
		if (SeekChunk(file, 0x54474856u)) {
			struct VHGT
			{
				float offset;
				std::int8_t deltas[1089];
				std::byte padding[3];
			} data{};
			static_assert(sizeof(data) == 1096);
			if (!ReadChunk(file, &data, sizeof(data)))
				return false;
			float offset = data.offset;
			for (std::size_t y = 0; y < 33; ++y) {
				float rowOffset = 0;
				offset += static_cast<float>(data.deltas[y * 33]);
				for (std::size_t x = 0; x < 33; ++x) {
					if (x != 0)
						rowOffset += static_cast<float>(data.deltas[y * 33 + x]);
					a_land = std::min(a_land, (rowOffset + offset) * 8.0f);
				}
			}
		}
		if (a_water == FLT_MAX)
			a_water = a_world->defaultWaterHeight;
		if (a_land == FLT_MAX)
			a_land = a_world->defaultLandHeight;
		return true;
	}
}
