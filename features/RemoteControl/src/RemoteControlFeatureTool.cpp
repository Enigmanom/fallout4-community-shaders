#include "RemoteControl.h"
#include "RemoteControlTools.h"

#include "Feature.h"
#include "Plugin.h"
#include "Settings/LiveSettings.h"
#include "Settings/SettingsJson.h"

#include <algorithm>
#include <exception>
#include <span>

namespace cs::features::remote_control
{
	namespace
	{
		Json FeatureEntry(const Feature& a_feature)
		{
			Json entry{
				{ "name", std::string(a_feature.GetDisplayName()) },
				{ "shortName", std::string(a_feature.GetName()) },
				{ "loaded", a_feature.IsLoaded() },
				{ "enabled", a_feature.IsEnabled() },
				{ "toggleable", a_feature.SupportsEnableToggle() },
				{ "version", Plugin::VERSION.string(".") },
				{ "category", a_feature.GetCategory() },
				{ "inMenu", a_feature.IsInMenu() },
			};
			const auto pending = a_feature.GetRestartSettings();
			auto restartFields = Json::array();
			for (const auto& field : a_feature.GetSettingsSchema()) {
				if (field.timing != settings::ApplyTiming::kNextLaunch)
					continue;
				restartFields.push_back({ { "key", field.key },
					{ "label", field.description },
					{ "pending", std::ranges::find(pending, field.description) != pending.end() } });
			}
			if (!restartFields.empty())
				entry["restartFields"] = std::move(restartFields);
			return entry;
		}

		// Everything below runs on the frame thread, where the host UI edits the same settings.

		Json List()
		{
			auto features = Json::array();
			for (const auto* feature : FeatureManager::Get().GetRegisteredFeatures())
				features.push_back(FeatureEntry(*feature));
			return features;
		}

		Json Get(const std::string& a_shortName)
		{
			auto* feature = FindFeature(a_shortName, true);
			if (!feature)
				return Error("feature not found or not loaded", { { "shortName", a_shortName } });
			const auto& access = feature->GetLiveSettingsAccess();
			if (!access.snapshot)
				return Error("feature exposes no live settings", { { "shortName", a_shortName } });
			return settings::TomlToJson(access.snapshot());
		}

		// Set and reset change live state, so they share the same preconditions.
		Json EditableFeature(const std::string& a_shortName, Feature*& a_feature)
		{
			a_feature = FindFeature(a_shortName, true);
			if (!a_feature)
				return Error("feature not found or not loaded", { { "shortName", a_shortName } });
			if (!a_feature->IsHealthy())
				return Error("feature is degraded or failed", { { "shortName", a_shortName } });
			if (settings::liveComparisonActive)
				return Error("an A/B comparison is active", { { "hint", "stop it in the Performance Overlay first" } });
			return {};
		}

		Json Set(const std::string& a_shortName, const Json& a_settings)
		{
			Feature* feature{};
			if (auto refusal = EditableFeature(a_shortName, feature); !refusal.is_null())
				return refusal;
			const auto& access = feature->GetLiveSettingsAccess();
			if (!access.snapshot || !access.prepare)
				return Error("feature exposes no live settings", { { "shortName", a_shortName } });

			const auto values = settings::JsonToToml(a_settings);
			if (!values)
				return Error(values.error(), { { "shortName", a_shortName } });
			const auto issues = settings::ClassifyKeys(*values, access.snapshot(), feature->GetSettingsSchema());
			if (!issues.empty()) {
				Json refusal{ { "shortName", a_shortName } };
				if (!issues.restartOnly.empty())
					refusal["restartOnly"] = issues.restartOnly;
				if (!issues.unknown.empty())
					refusal["unknown"] = issues.unknown;
				return Error(issues.restartOnly.empty() ? "unknown settings keys" : "restart-only settings cannot apply live", std::move(refusal));
			}

			std::string error;
			auto prepared = FeatureManager::Get().PrepareLiveSettings(*feature, *values, "RemoteControl::Set", error);
			if (!prepared)
				return Error(std::move(error), { { "shortName", a_shortName } });
			try {
				settings::ApplyPreparedLiveSettings(std::span{ &*prepared, 1 });
			} catch (const std::exception& failure) {
				return Error("settings could not be applied", { { "shortName", a_shortName }, { "detail", failure.what() } });
			}
			return { { "action", "set" }, { "shortName", a_shortName }, { "applied", true } };
		}

		Json Reset(const std::string& a_shortName)
		{
			Feature* feature{};
			if (auto refusal = EditableFeature(a_shortName, feature); !refusal.is_null())
				return refusal;
			try {
				feature->RestoreDefaults();
			} catch (const std::exception& failure) {
				return Error("reset failed", { { "shortName", a_shortName }, { "detail", failure.what() } });
			}
			// The framework reset saves as part of restoring; there is no live-only variant.
			return { { "action", "reset" }, { "shortName", a_shortName }, { "applied", true }, { "persisted", true } };
		}

		Json Toggle(const std::string& a_shortName, const Json& a_enabled)
		{
			auto* feature = FindFeature(a_shortName, false);
			if (!feature)
				return Error("unknown shortName", { { "shortName", a_shortName } });
			if (!feature->IsLoaded()) {
				return Error("feature is not loaded", { { "shortName", a_shortName },
														  { "hint", "enable Load on startup in Advanced and restart" } });
			}
			if (!feature->SupportsEnableToggle())
				return Error("feature does not support toggling", { { "shortName", a_shortName } });
			if (!feature->IsHealthy())
				return Error("feature is degraded or failed", { { "shortName", a_shortName } });
			if (settings::liveComparisonActive)
				return Error("an A/B comparison is active", { { "hint", "stop it in the Performance Overlay first" } });

			const bool enabled = a_enabled.is_boolean() ? a_enabled.get<bool>() : !feature->IsEnabled();
			feature->SetEnabled(enabled);
			auto* self = RemoteControl::GetSingleton();
			// Its own disable is the one change the mute would otherwise swallow.
			self->EmitEvent("communityshaders.feature.changed",
				{ { "shortName", a_shortName }, { "enabled", enabled } },
				feature == self);
			return { { "action", "toggle" }, { "shortName", a_shortName }, { "enabled", enabled } };
		}

		Json Handle(const Json& a_args)
		{
			const auto action = a_args.value("action", std::string("list"));
			if (action == "list")
				return OnFrame([] { return List(); });

			if (action != "get" && action != "set" && action != "reset" && action != "toggle")
				return Error("unknown action (list|get|set|reset|toggle)", { { "action", action } });
			const auto shortName = a_args.value("shortName", std::string{});
			if (shortName.empty())
				return Error("missing required parameter 'shortName'", { { "action", action } });

			if (action == "get")
				return OnFrame([shortName] { return Get(shortName); });
			if (action == "reset")
				return OnFrame([shortName] { return Reset(shortName); });
			if (action == "toggle") {
				const auto enabled = a_args.value("enabled", Json());
				if (!enabled.is_null() && !enabled.is_boolean())
					return Error("'enabled' must be a boolean");
				return OnFrame([shortName, enabled] { return Toggle(shortName, enabled); });
			}
			if (!a_args.contains("settings") || !a_args["settings"].is_object())
				return Error("missing required object parameter 'settings'");
			return OnFrame([shortName, values = a_args["settings"]] { return Set(shortName, values); });
		}
	}

	const Tool kFeatureTool{
		"communityshaders.feature",
		R"({"description":"Community Shaders feature operations. Action-dispatched. list (default): every registered feature as {name,shortName,loaded,enabled,toggleable,version,category,inMenu}; features with next-launch settings add restartFields:[{key,label,pending}]. get: params shortName; returns the live settings and the framework 'enabled' switch of a loaded feature as an object (settings that apply only at restart are omitted). set: params shortName, settings (object); applies live to a loaded feature and does NOT persist (use communityshaders.settings save); unknown keys and restart-only keys are refused with restartOnly/unknown lists and nothing is applied. reset: params shortName; the framework restore-defaults including 'enabled', which also saves. toggle: params shortName, enabled (boolean, optional; omit to flip); flips the framework 'enabled' switch of a loaded feature that supports it, live and not persisted; emits communityshaders.feature.changed.","inputSchema":{"type":"object","properties":{"action":{"type":"string","enum":["list","get","set","reset","toggle"]},"shortName":{"type":"string"},"settings":{"type":"object"},"enabled":{"type":"boolean"}}}})",
		&Handle
	};
}
