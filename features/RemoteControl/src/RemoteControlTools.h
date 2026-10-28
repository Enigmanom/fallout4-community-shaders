#pragma once

#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace DevBenchAPI
{
	struct IDevBenchInterface001;
}

namespace cs
{
	class Feature;
}

namespace cs::features::remote_control
{
	using Json = nlohmann::json;

	using ToolHandler = Json (*)(const Json& a_args);

	struct Tool
	{
		const char* name;
		const char* descriptor;
		ToolHandler handler;
	};

	extern const Tool kFeatureTool;
	extern const Tool kInspectTool;
	extern const Tool kShaderCacheTool;
	extern const Tool kSettingsTool;
	extern const Tool kCaptureTool;
	extern const Tool kNvPerfTool;

	[[nodiscard]] std::span<const Tool* const> GetTools() noexcept;

	void SubscribeShaderCacheEvents();
	void SubscribeCaptureEvents();
	void SubscribeNvPerfEvents();

	// Host settings page section for the Nsight Perf profiler.
	void DrawNvPerfPanel();

	std::vector<std::string_view> RegisterTools(DevBenchAPI::IDevBenchInterface001& a_host);

	[[nodiscard]] Feature* FindFeature(std::string_view a_shortName, bool a_loadedOnly);
	[[nodiscard]] Json Error(std::string a_message, Json a_details = Json::object());
	[[nodiscard]] std::string PathToUtf8(const std::filesystem::path& a_path);
	[[nodiscard]] Json OnFrame(std::function<Json()> a_task);
}
