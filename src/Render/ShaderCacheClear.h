#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace cs::engine
{
	enum class ShaderCacheScope : std::uint8_t
	{
		kMemory,
		kMemoryAndDisk
	};

	struct ShaderCacheClearResult
	{
		bool succeeded{};
		std::uintmax_t removed{};
		std::filesystem::path root;
		std::string error;
	};

	// Disk goes first so a failure keeps the compiled variants.
	ShaderCacheClearResult ClearShaderCache(ShaderCacheScope a_scope);
}
