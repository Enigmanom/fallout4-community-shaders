#pragma once

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

	// Runs on DevBench's listener thread and returns its result; failures are in-band {error, ...}.
	using ToolHandler = Json (*)(const Json& a_args);

	struct Tool
	{
		const char* name;
		// JSON: { description, readOnly?, inputSchema }.
		const char* descriptor;
		ToolHandler handler;
	};

	// One definition per tool file; adding a tool is a new file plus an entry in GetTools.
	extern const Tool kFeatureTool;
	extern const Tool kInspectTool;
	extern const Tool kShaderCacheTool;
	extern const Tool kSettingsTool;
	extern const Tool kCaptureTool;

	[[nodiscard]] std::span<const Tool* const> GetTools() noexcept;

	// Event sources owned by a tool; they publish through RemoteControl::EmitEvent.
	void SubscribeShaderCacheEvents();
	void SubscribeCaptureEvents();

	// Registers every tool with the host; returns the names it registered.
	std::vector<std::string_view> RegisterTools(DevBenchAPI::IDevBenchInterface001& a_host);

	// Matches the feature's GetName(); a loaded-only lookup skips features that failed or are off.
	[[nodiscard]] Feature* FindFeature(std::string_view a_shortName, bool a_loadedOnly);
	[[nodiscard]] Json Error(std::string a_message, Json a_details = Json::object());
	// Runs a_task at the next frame boundary; a dispatcher failure becomes an {error} result.
	[[nodiscard]] Json OnFrame(std::function<Json()> a_task);
}
