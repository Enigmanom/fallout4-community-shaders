#pragma once

#include "Render/ShaderInjection.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace cs::engine
{
	[[nodiscard]] std::string ProfileForStage(ShaderStage a_stage);
	[[nodiscard]] std::filesystem::path GetShaderPath(std::string_view a_sourceName);
	[[nodiscard]] bool IsShaderSourceAvailable(
		const std::filesystem::path& a_shaderRoot, std::string_view a_sourceName);

	struct ShaderFamilyDescriptor
	{
		ShaderInjectionTarget target = ShaderInjectionTarget::kCount;
		ShaderStage stage = ShaderStage::kPixel;
		// Native stage-map ID, not the raw SetupTechnique value.
		std::uint32_t descriptor = 0;
		std::string_view nativeName;
		std::string_view nativeClassName;
		std::string_view nativeSourceGroup;
		bool forceEarlyDepthStencil = false;
		ShaderInjectionDefines nativeMacros;
	};

	// Source file that reconstructs a native shader; usually its own name.
	[[nodiscard]] std::string_view ResolveShaderSourceName(
		const ShaderFamilyDescriptor& a_descriptor) noexcept;

	[[nodiscard]] std::optional<ShaderVariantCompilationDescriptor>
	BuildShaderFamilyCompilationDescriptor(
		const ShaderFamilyDescriptor& a_descriptor);
}
