#pragma once

#include "Render/ShaderStage.h"

#include <d3d11.h>
#include <winrt/base.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cs::engine
{
	enum class ShaderVariantCompilationState : std::uint8_t
	{
		kReady,
		kPending,
		kFailed
	};

	struct ShaderVariantCompilationRequest
	{
		winrt::com_ptr<ID3D11Device> device;
		std::filesystem::path sourcePath;
		std::string entryPoint;
		std::string profile;
		ShaderStage stage = ShaderStage::kPixel;
		std::vector<std::pair<std::string, std::string>> defines;
		std::uint32_t familyId = 0xFFFFFFFFU;
		std::uint32_t descriptor = 0;
		std::string owner;
		std::uint64_t sourceGeneration = 0;
	};

	class ShaderVariantCompilationHandle
	{
	public:
		virtual ~ShaderVariantCompilationHandle() = default;

		virtual ShaderVariantCompilationState GetState() const noexcept = 0;
		virtual winrt::com_ptr<ID3D11DeviceChild> Acquire() noexcept = 0;
		virtual std::string GetError() const = 0;
		virtual std::uint16_t GetSamplerMask() const noexcept { return UINT16_MAX; }
		virtual bool BindsHostSampler() const noexcept { return false; }
	};

	struct ShaderVariantCompilationOutput
	{
		winrt::com_ptr<ID3D11DeviceChild> shader;
		std::string error;
		std::uint16_t samplerMask = UINT16_MAX;
		bool bindsHostSampler = false;
	};

	using ShaderVariantCompiler =
		std::function<ShaderVariantCompilationOutput(
			ShaderVariantCompilationRequest)>;

	class ShaderVariantCompilationCache
	{
	public:
		virtual ~ShaderVariantCompilationCache() = default;

		virtual std::shared_ptr<ShaderVariantCompilationHandle> Request(
			ShaderVariantCompilationRequest a_request) = 0;
		virtual void Invalidate() = 0;
		virtual void Stop() noexcept = 0;
	};

	// A batch runs from the first queued variant to the last one finishing.
	struct ShaderCompileBatch
	{
		std::uint32_t completedTasks = 0;
		std::uint32_t failedTasks = 0;
		std::uint32_t totalTasks = 0;
		std::chrono::milliseconds duration{};
		bool active = false;
	};

	using ShaderCompileBatchObserver = std::function<void(const ShaderCompileBatch&)>;

	// Observers run on compile worker threads and must not block.
	void AddShaderCompileBatchObserver(ShaderCompileBatchObserver a_observer);
	ShaderCompileBatch GetShaderCompileBatch();

	std::shared_ptr<ShaderVariantCompilationCache>
	CreateAsyncShaderVariantCompilationCache(
		ShaderVariantCompiler a_compiler,
		std::size_t a_workerCount = 1);

	std::shared_ptr<ShaderVariantCompilationCache>
	CreateCachingShaderVariantCompilationCache();
}
