#include "WaterTiles.h"

#include <iostream>
#include <limits>
#include <stdexcept>

using namespace cs::features::unified_water;

namespace
{
	void Require(bool a_pass, const char* a_message)
	{
		if (!a_pass)
			throw std::runtime_error(a_message);
	}

	void CheckCoverage(Bounds a_bounds, const std::vector<CellData>& a_cells, const std::vector<Instruction>& a_tiles)
	{
		const auto width = a_bounds.maxX - a_bounds.minX + 1;
		std::vector<unsigned> covered(a_cells.size());
		for (const auto& tile : a_tiles) {
			Require(GetLODCoord(tile.lodLevel, tile.x) == GetLODCoord(tile.lodLevel, tile.x + tile.size - 1) &&
						GetLODCoord(tile.lodLevel, tile.y) == GetLODCoord(tile.lodLevel, tile.y + tile.size - 1),
				"merged water crossed a native chunk boundary");
			for (std::uint32_t y = 0; y < tile.size; ++y)
				for (std::uint32_t x = 0; x < tile.size; ++x) {
					const auto xx = tile.x + static_cast<std::int32_t>(x) - a_bounds.minX;
					const auto yy = tile.y + static_cast<std::int32_t>(y) - a_bounds.minY;
					Require(xx >= 0 && xx < width && yy >= 0 && yy <= a_bounds.maxY - a_bounds.minY,
						"tile escaped the world bounds");
					const auto index = yy * width + xx;
					const auto& cell = a_cells[index];
					const auto& origin = a_cells[(tile.y - a_bounds.minY) * width + tile.x - a_bounds.minX];
					Require(cell.formID == tile.form.id && cell.water == tile.waterHeight && cell.flags == origin.flags,
						"merged incompatible height, form or flags");
					++covered[index];
				}
		}
		for (std::size_t i = 0; i < a_cells.size(); ++i)
			Require(covered[i] == (a_cells[i].formID ? 1u : 0u), "water cell was omitted or covered more than once");
	}
}

int main()
{
	try {
		Bounds bounds{ -5, -3, 5, 4 };
		std::vector<CellData> cells(11 * 8, CellData{ 450.0f, 1, 0 });
		cells[5].formID = 0;
		cells[20].formID = 2;
		cells[40].water = 451.0f;
		cells[60].flags = 1;
		for (const auto level : { 4, 8, 16, 32 }) {
			std::vector<Instruction> tiles;
			GenerateInstructions(level, bounds, cells, tiles);
			CheckCoverage(bounds, cells, tiles);
			if (level == 4)
				Require(std::ranges::all_of(tiles, [](const auto& tile) { return tile.size == 1; }),
					"LOD4 merging prevents individual loaded-cell visibility");
		}
		std::vector<Instruction> merged;
		GenerateInstructions(8, { 0, 0, 7, 7 }, std::vector<CellData>(64, CellData{ 450.0f, 1, 0 }), merged);
		Require(merged.size() == 1 && merged.front().size == 8, "uniform LOD8 was not greedily merged");

		CacheHeader header{ .width = 8, .height = 8, .bounds = { 0, 0, 7, 7 }, .dataCount = 1 };
		const auto complete = sizeof(header) + sizeof(Instruction);
		Require(IsValidCacheHeader(header, complete), "valid generated cache rejected");
		Require(!IsValidCacheHeader(header, complete - 1), "truncated cache accepted");
		header.bounds.maxX = std::numeric_limits<std::int32_t>::max();
		Require(!IsValidCacheHeader(header, complete), "corrupt bounds could cause oversized allocation");
		header.bounds.maxX = 6;
		Require(!IsValidCacheHeader(header, complete), "inconsistent cache dimensions accepted");
		std::cout << "Unified Water coverage, merge barriers and cache corruption checks passed\n";
		return 0;
	} catch (const std::exception& error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
