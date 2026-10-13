#include "Settings/SettingsJson.h"

#include <algorithm>
#include <limits>
#include <memory>

namespace cs::settings
{
	namespace
	{
		using Node = std::unique_ptr<toml::node>;
		using Json = nlohmann::json;

		std::expected<Node, std::string> ToNode(const Json& a_value, const std::string& a_path)
		{
			switch (a_value.type()) {
			case Json::value_t::boolean:
				return Node{ new toml::value<bool>(a_value.get<bool>()) };
			case Json::value_t::number_integer:
				return Node{ new toml::value<std::int64_t>(a_value.get<std::int64_t>()) };
			case Json::value_t::number_unsigned:
				if (a_value.get<std::uint64_t>() > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
					return std::unexpected(a_path + ": integer is out of range");
				return Node{ new toml::value<std::int64_t>(a_value.get<std::int64_t>()) };
			case Json::value_t::number_float:
				return Node{ new toml::value<double>(a_value.get<double>()) };
			case Json::value_t::string:
				return Node{ new toml::value<std::string>(a_value.get<std::string>()) };
			case Json::value_t::array:
				{
					auto array = std::make_unique<toml::array>();
					for (std::size_t i = 0; i < a_value.size(); ++i) {
						auto element = ToNode(a_value[i], a_path + "[" + std::to_string(i) + "]");
						if (!element)
							return std::unexpected(std::move(element.error()));
						array->push_back(**element);
					}
					return Node{ std::move(array) };
				}
			case Json::value_t::object:
				{
					auto table = std::make_unique<toml::table>();
					for (const auto& [key, member] : a_value.items()) {
						auto node = ToNode(member, a_path + "." + key);
						if (!node)
							return std::unexpected(std::move(node.error()));
						table->insert_or_assign(key, **node);
					}
					return Node{ std::move(table) };
				}
			default:
				return std::unexpected(a_path + ": null is not a settings value");
			}
		}

		Json ToJson(const toml::node& a_node)
		{
			if (const auto* table = a_node.as_table()) {
				auto object = Json::object();
				for (const auto& [key, member] : *table)
					object[std::string(key.str())] = ToJson(member);
				return object;
			}
			if (const auto* array = a_node.as_array()) {
				auto elements = Json::array();
				for (const auto& element : *array)
					elements.push_back(ToJson(element));
				return elements;
			}
			if (const auto* boolean = a_node.as_boolean())
				return boolean->get();
			if (const auto* integer = a_node.as_integer())
				return integer->get();
			if (const auto* number = a_node.as_floating_point())
				return number->get();
			if (const auto* text = a_node.as_string())
				return text->get();
			return nullptr;
		}
	}

	std::expected<toml::table, std::string> JsonToToml(const Json& a_object)
	{
		if (!a_object.is_object())
			return std::unexpected("settings must be a JSON object");
		auto node = ToNode(a_object, "settings");
		if (!node)
			return std::unexpected(std::move(node.error()));
		return std::move(*(*node)->as_table());
	}

	Json TomlToJson(const toml::table& a_table)
	{
		return ToJson(a_table);
	}

	KeyIssues ClassifyKeys(
		const toml::table& a_values,
		const toml::table& a_liveSnapshot,
		const SchemaView& a_schema)
	{
		KeyIssues issues;
		for (const auto& [key, node] : a_values) {
			(void)node;
			if (a_liveSnapshot.contains(key))
				continue;
			const auto field = std::ranges::find(a_schema, key.str(), &FieldView::key);
			const bool restartOnly = field != a_schema.end() && field->timing == ApplyTiming::kNextLaunch;
			(restartOnly ? issues.restartOnly : issues.unknown).emplace_back(key.str());
		}
		return issues;
	}
}
