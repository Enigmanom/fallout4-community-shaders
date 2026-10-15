#pragma once

#ifndef NOMINMAX
#	define NOMINMAX
#endif
#include <Windows.h>

#include "Utils/ShaderCache/RevalidationContext.h"
#include "Utils/ShaderCache/ShaderCache.h"
#include "Utils/ShaderCache/SourceCompile.h"

#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>

// Test compiles use the runtime shader cache when this names a directory.
namespace cs::shader_test
{
	inline std::optional<std::filesystem::path> CacheRoot()
	{
		static const auto root = [] {
			std::wstring value(MAX_PATH, L'\0');
			const auto length = GetEnvironmentVariableW(L"FO4CS_SHADER_TEST_CACHE", value.data(), static_cast<DWORD>(value.size()));
			if (length > value.size()) {
				value.resize(length);
				GetEnvironmentVariableW(L"FO4CS_SHADER_TEST_CACHE", value.data(), static_cast<DWORD>(value.size()));
			}
			value.resize(length < value.size() ? length : value.size() - 1);
			return value.empty() ? std::optional<std::filesystem::path>{} : std::optional(std::filesystem::path(value));
		}();
		return root;
	}

	// Hits are revalidated against the include trace; only D3DCompile is skipped.
	inline shader_cache::ShaderCacheOutcome Compile(const shader_cache::ShaderRecipe& a_recipe)
	{
		const auto root = CacheRoot();
		if (!root) {
			auto compiled = shader_cache::CompileSourceWithManifest(a_recipe);
			shader_cache::ShaderCacheOutcome outcome;
			outcome.succeeded = compiled.succeeded;
			outcome.bytecode = std::move(compiled.bytecode);
			outcome.error = std::move(compiled.error);
			return outcome;
		}
		static shader_cache::RevalidationContext revalidation;
		return shader_cache::LoadOrCompileShader(a_recipe, { .cacheRoot = *root, .revalidation = &revalidation });
	}

	inline void ReportCache()
	{
		if (!CacheRoot())
			return;
		const auto counters = shader_cache::GetShaderCacheCounters();
		std::printf("Shader cache: %llu hit, %llu absent, %llu stale, %llu rejected, %llu written\n",
			counters.hit, counters.absent, counters.stale, counters.rejected, counters.written);
	}
}
