#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace cs::features::unified_water
{
	struct Instruction
	{
		std::uint32_t lodLevel{};
		union Form
		{
			std::uint32_t id;
			void* ptr = nullptr;
		} form{};
		std::int32_t x{};
		std::int32_t y{};
		std::uint32_t size{};
		float waterHeight{};
	};
	static_assert(sizeof(Instruction) == 32);

	struct Bounds
	{
		std::int32_t minX{};
		std::int32_t minY{};
		std::int32_t maxX{};
		std::int32_t maxY{};
	};

	struct CellData
	{
		float water{};
		std::uint32_t formID{};
		std::uint8_t flags{};
	};

	struct CacheHeader
	{
		std::uint32_t label = 0x48435457;
		std::int32_t width{};
		std::int32_t height{};
		Bounds bounds{};
		std::int32_t dataCount{};
	};

	inline bool IsValidCacheHeader(const CacheHeader& a_header, std::uintmax_t a_length)
	{
		const auto& b = a_header.bounds;
		return a_header.label == CacheHeader{}.label && a_header.width > 0 && a_header.height > 0 &&
		       a_header.width <= 512 && a_header.height <= 512 && b.minX >= -512 && b.minY >= -512 &&
		       b.maxX <= 511 && b.maxY <= 511 &&
		       static_cast<std::int64_t>(b.maxX) - b.minX + 1 == a_header.width &&
		       static_cast<std::int64_t>(b.maxY) - b.minY + 1 == a_header.height &&
		       a_header.dataCount >= 0 && a_length >= sizeof(CacheHeader) &&
		       static_cast<std::uintmax_t>(a_header.dataCount) <= (a_length - sizeof(CacheHeader)) / sizeof(Instruction);
	}

	inline std::int32_t GetLODCoord(std::int32_t a_level, std::int32_t a_cell)
	{
		return a_cell >= 0 ? a_cell / a_level : (a_cell - (a_level - 1)) / a_level;
	}

	inline void GenerateInstructions(std::int32_t lodLevel, Bounds bounds,
		const std::vector<CellData>& cellData, std::vector<Instruction>& instructions)
	{
		const auto [minX, minY, maxX, maxY] = bounds;
		const auto width = maxX - minX + 1;
		const auto height = maxY - minY + 1;
		const auto lodMinX = GetLODCoord(lodLevel, minX);
		const auto lodMinY = GetLODCoord(lodLevel, minY);
		const auto lodMaxX = GetLODCoord(lodLevel, maxX);
		const auto lodMaxY = GetLODCoord(lodLevel, maxY);
		std::vector<bool> processed(lodLevel * lodLevel);
		for (auto lodY = lodMinY; lodY <= lodMaxY; ++lodY) {
			for (auto lodX = lodMinX; lodX <= lodMaxX; ++lodX) {
				const auto baseX = lodX * lodLevel - minX;
				const auto baseY = lodY * lodLevel - minY;
				std::ranges::fill(processed, false);
				for (std::int32_t localY = 0; localY < lodLevel; ++localY) {
					for (std::int32_t localX = 0; localX < lodLevel; ++localX) {
						const auto localIndex = static_cast<std::size_t>(localY * lodLevel + localX);
						if (processed[localIndex])
							continue;
						const auto cellX = baseX + localX;
						const auto cellY = baseY + localY;
						if (cellX < 0 || cellX >= width || cellY < 0 || cellY >= height) {
							processed[localIndex] = true;
							continue;
						}
						const auto& target = cellData[cellY * width + cellX];
						if (!target.formID) {
							processed[localIndex] = true;
							continue;
						}
						std::int32_t size = 1;
						std::int32_t maxSize = 1;
						if (lodLevel > 4) {
							// LOD4 stays per-cell; larger LODs respect chunk/map bounds.
							maxSize = std::min({ lodLevel - localX, lodLevel - localY, width - cellX, height - cellY });
						}
						const auto matches = [&](std::int32_t x, std::int32_t y) {
							const auto& candidate = cellData[(baseY + y) * width + baseX + x];
							return !processed[y * lodLevel + x] && candidate.formID &&
							       candidate.water == target.water && candidate.formID == target.formID && candidate.flags == target.flags;
						};
						// Try to grow to the largest square possible starting at localX/localY
						for (std::int32_t candidateSize = 2; candidateSize <= maxSize; ++candidateSize) {
							bool failed = false;
							const auto row = localY + candidateSize - 1;
							for (std::int32_t dx = 0; dx < candidateSize; ++dx)
								if (!matches(localX + dx, row)) {
									failed = true;
									break;
								}
							if (failed)
								break;
							// Exclude the corner already checked in the top row.
							const auto column = localX + candidateSize - 1;
							for (std::int32_t dy = 0; dy < candidateSize - 1; ++dy)
								if (!matches(column, localY + dy)) {
									failed = true;
									break;
								}
							if (failed)
								break;
							size = candidateSize;
						}
						for (auto yy = localY; yy < localY + size; ++yy)
							for (auto xx = localX; xx < localX + size; ++xx)
								processed[yy * lodLevel + xx] = true;
						Instruction instruction;
						instruction.lodLevel = lodLevel;
						instruction.form.id = target.formID;
						instruction.x = cellX + minX;
						instruction.y = cellY + minY;
						instruction.size = size;
						instruction.waterHeight = target.water;
						instructions.push_back(instruction);
					}
				}
			}
		}
	}
}
