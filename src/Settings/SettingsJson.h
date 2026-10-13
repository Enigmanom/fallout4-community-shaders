#pragma once

#include "Settings/SettingsMetadata.h"

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <toml++/toml.hpp>

// The wire format of remote settings; the TOML snapshot stays the only settings representation.
namespace cs::settings
{
	[[nodiscard]] std::expected<toml::table, std::string> JsonToToml(const nlohmann::json& a_object);
	[[nodiscard]] nlohmann::json TomlToJson(const toml::table& a_table);

	struct KeyIssues
	{
		std::vector<std::string> unknown;
		std::vector<std::string> restartOnly;

		[[nodiscard]] bool empty() const noexcept { return unknown.empty() && restartOnly.empty(); }
	};

	// A key applies live only if the feature's live snapshot carries it.
	[[nodiscard]] KeyIssues ClassifyKeys(
		const toml::table& a_values,
		const toml::table& a_liveSnapshot,
		const SchemaView& a_schema);
}
