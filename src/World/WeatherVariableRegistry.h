#pragma once

#include "Settings/SettingsSchema.h"
#include "World/Weather.h"
#include "World/WeatherProfiles.h"

#include <algorithm>
#include <charconv>
#include <map>
#include <mutex>
#include <set>

namespace cs::weather
{
	// FO4: typed weather profiles feed the same portable settings as the live user schema.
	template <class Schema>
	class VariableRegistry final : public Profiles
	{
	public:
		using Settings = typename Schema::SettingsType;

		constexpr VariableRegistry(const Schema& a_schema, std::span<const std::string_view> a_keys) :
			_schema(a_schema), _keys(a_keys)
		{}

		bool Configure(const toml::node* a_node, std::string& a_error)
		{
			std::map<std::string, Override, std::less<>> candidate;
			if (a_node) {
				const auto* table = a_node->as_table();
				if (!table) {
					a_error = "weather: expected a table keyed by '0xLocalFormID~Plugin.ext'";
					return false;
				}
				for (const auto& [key, node] : *table) {
					const auto identity = ParseIdentity(key.str());
					const auto* values = node.as_table();
					if (identity.empty() || !values || candidate.contains(identity)) {
						a_error = "weather." + std::string(key.str()) + ": expected a unique '0xLocalFormID~Plugin.ext' and settings table";
						return false;
					}
					Override value;
					if (!ParseOverride(*values, value, a_error)) {
						a_error = "weather." + std::string(key.str()) + "." + a_error;
						return false;
					}
					candidate.emplace(identity, std::move(value));
				}
			}
			const std::scoped_lock lock(_mutex);
			_overrides = std::move(candidate);
			_initialized = false;
			return true;
		}

		// FO4: the engine clears previous weather before the transition ends.
		Settings EvaluateCurrentWeather(const Settings& a_base)
		{
			const auto weather = engine::SnapshotWeather();
			const auto current = engine::WeatherKey(weather.current);
			if (weather.previous)
				_enginePrevious = engine::WeatherKey(weather.previous);
			return Evaluate(a_base, _enginePrevious, current, weather.transitionPct);
		}

		Settings Evaluate(const Settings& a_base, std::string_view a_previous, std::string_view a_current, float a_factor)
		{
			const std::scoped_lock lock(_mutex);
			const auto* from = Find(a_previous);
			const auto* to = Find(a_current);
			const bool begin = !_initialized || a_current != _current || a_previous != _previous || a_factor < _factor;
			if (begin)
				_start = _initialized ? _live : a_base;
			Settings result = a_base;
			std::apply([&](const auto&... fields) {
				const auto apply = [&](const auto& field) {
					const bool hasFrom = from && from->keys.contains(field.key);
					const bool hasTo = to && to->keys.contains(field.key);
					if (!hasFrom && !hasTo)
						return;
					if (begin && hasFrom)
						_start.*field.member = from->values.*field.member;
					const auto& first = _start.*field.member;
					const auto& last = hasTo ? to->values.*field.member : a_base.*field.member;
					auto& value = result.*field.member;
					using T = typename std::remove_cvref_t<decltype(field)>::ValueType;
					if constexpr (std::same_as<T, settings::Float3> || std::same_as<T, settings::Color4>) {
						for (std::size_t i = 0; i < value.size(); ++i)
							value[i] = std::lerp(first[i], last[i], a_factor);
					} else if constexpr (std::floating_point<T>) {
						value = std::lerp(first, last, a_factor);
					} else {
						value = a_factor > 0.5f ? last : first;
					}
				};
				(apply(fields), ...);
			},
				_schema.fields);
			_previous = a_previous;
			_current = a_current;
			_factor = a_factor;
			_initialized = true;
			_live = result;
			return result;
		}

		settings::SchemaView Variables() const override
		{
			const auto all = settings::MakeSchemaView(_schema);
			settings::SchemaView result;
			for (const auto key : _keys) {
				if (const auto found = std::ranges::find(all, key, &settings::FieldView::key); found != all.end())
					result.push_back(*found);
			}
			return result;
		}

		bool IsActive(std::string_view a_weather) const override
		{
			const std::scoped_lock lock(_mutex);
			return Find(a_weather) != nullptr;
		}

		std::optional<toml::table> Get(std::string_view a_weather) const override
		{
			const std::scoped_lock lock(_mutex);
			const auto found = _overrides.find(a_weather);
			if (found == _overrides.end())
				return std::nullopt;
			return SerializeOverride(found->second);
		}

		bool Set(std::string_view a_weather, const toml::table* a_profile, std::string& a_error) override
		{
			const auto identity = ParseIdentity(a_weather);
			if (identity.empty()) {
				a_error = "weather." + std::string(a_weather) + ": expected '0xLocalFormID~Plugin.ext'";
				return false;
			}
			std::optional<Override> value;
			if (a_profile) {
				value.emplace();
				if (!ParseOverride(*a_profile, *value, a_error)) {
					a_error = "weather." + identity + "." + a_error;
					return false;
				}
			}
			const std::scoped_lock lock(_mutex);
			if (value)
				_overrides.insert_or_assign(identity, std::move(*value));
			else
				_overrides.erase(identity);
			return true;
		}

		toml::table Serialize() const override
		{
			const std::scoped_lock lock(_mutex);
			toml::table table;
			for (const auto& [identity, value] : _overrides)
				table.insert(identity, SerializeOverride(value));
			return table;
		}

	private:
		struct Override
		{
			bool enabled = false;
			Settings values;
			std::set<std::string, std::less<>> keys;
		};

		// Normalizes "0xID~Plugin.ext"; empty when malformed.
		static std::string ParseIdentity(std::string_view a_text)
		{
			std::uint32_t id = 0;
			const auto separator = a_text.find('~');
			const auto plugin = separator != std::string_view::npos ? a_text.substr(separator + 1) : std::string_view{};
			auto text = a_text.substr(0, separator);
			if (text.starts_with("0x") || text.starts_with("0X"))
				text.remove_prefix(2);
			const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), id, 16);
			if (ec != std::errc{} || end != text.data() + text.size() || id == 0 || id > 0xFFFFFF || plugin.empty())
				return {};
			return engine::FormatWeatherKey(id, plugin);
		}

		bool ParseOverride(const toml::table& a_values, Override& a_value, std::string& a_error) const
		{
			if (const auto* enabled = a_values.get(kEnabledKey)) {
				if (!enabled->is_boolean()) {
					a_error = std::string(kEnabledKey) + ": expected boolean";
					return false;
				}
				a_value.enabled = enabled->value<bool>().value();
			}
			auto settingsTable = a_values;
			settingsTable.erase(kEnabledKey);
			for (const auto& [name, entry] : settingsTable) {
				(void)entry;
				if (std::ranges::find(_keys, name.str()) == _keys.end()) {
					a_error = std::string(name.str()) + ": not a weather variable";
					return false;
				}
				a_value.keys.emplace(name.str());
			}
			return settings::ParseTable(_schema, settingsTable, a_value.values, a_error);
		}

		toml::table SerializeOverride(const Override& a_value) const
		{
			toml::table table;
			table.insert(kEnabledKey, a_value.enabled);
			std::apply([&](const auto&... fields) {
				const auto write = [&](const auto& field) {
					if (a_value.keys.contains(field.key))
						field.Write(table, a_value.values);
				};
				(write(fields), ...);
			},
				_schema.fields);
			return table;
		}

		const Override* Find(std::string_view a_id) const
		{
			const auto found = _overrides.find(a_id);
			return found != _overrides.end() && found->second.enabled ? &found->second : nullptr;
		}

		const Schema& _schema;
		std::span<const std::string_view> _keys;
		mutable std::mutex _mutex;
		std::map<std::string, Override, std::less<>> _overrides;
		Settings _start{}, _live{};
		std::string _previous, _current, _enginePrevious;
		float _factor = 1;
		bool _initialized = false;
	};
}
