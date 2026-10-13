#pragma once

#include "Feature.h"
#include "FeatureCategories.h"

#include <atomic>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace DevBenchAPI
{
	struct IDevBenchInterface001;
}

namespace cs::features
{
	// Thin client of the external DevBench host: it registers tools there and runs no server.
	class RemoteControl : public Feature
	{
	public:
		static RemoteControl* GetSingleton();

		std::string_view GetName() const override { return "RemoteControl"; }
		std::string_view GetDisplayName() const override { return "Remote Control"; }
		std::string GetCategory() const override { return FeatureCategories::kDevTools; }
		std::string GetFeatureSummary() const override
		{
			return "Expose Community Shaders tools to the DevBench host for MCP and REST clients.";
		}

		void OnPostPostLoad() override;
		void DrawSettings() override;

		// Any thread. Silent until DevBench is connected and while the feature is disabled; tools
		// cannot be unregistered, so a disabled feature answers them with an error instead.
		// a_bypassMute announces Remote Control's own disable, which happens after the mute begins.
		void EmitEvent(const char* a_topic, const nlohmann::json& a_payload, bool a_bypassMute = false) const;

	private:
		RemoteControl() = default;

		void SubscribeToEvents();
		[[nodiscard]] int ReadHostPort();

		std::atomic<DevBenchAPI::IDevBenchInterface001*> _host{ nullptr };
		unsigned int _hostBuild{};
		std::vector<std::string_view> _registeredTools;

		int _cachedPort{ -1 };
		std::chrono::steady_clock::time_point _portReadAt;
	};
}
