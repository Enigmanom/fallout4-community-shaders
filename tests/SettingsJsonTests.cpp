#include "Settings/LiveSettings.h"
#include "Settings/SettingsJson.h"
#include "Settings/SettingsSchema.h"

#include "Check.h"

#include <iostream>
#include <string_view>

namespace
{
	using namespace cs::settings;
	using nlohmann::json;

	struct TestSettings
	{
		bool enabled = true;
		std::uint32_t count = 2;
		float thickness = 0.01f;
		std::string label = "a";
		Float3 tint{ 1.0f, 0.5f, 0.25f };
		std::uint32_t boot = 1;
	};

	constexpr Schema kSchema{
		std::tuple{
			Field{ "enabled", "Enable.", &TestSettings::enabled },
			Field{ "count", "Count.", &TestSettings::count, Range{ 1u, 4u } },
			Field{ "thickness", "Thickness.", &TestSettings::thickness, Range{ 0.0f, 4.0f } },
			Field{ "label", "Label.", &TestSettings::label },
			Float3Field<TestSettings>{ "tint", "Tint.", &TestSettings::tint },
			Field{ "boot", "Boot.", &TestSettings::boot, Range{ 0u, 9u }, ApplyTiming::kNextLaunch } }
	};

	std::expected<std::monostate, std::string> Apply(LiveSettingsAccess& a_access, const json& a_request)
	{
		auto values = JsonToToml(a_request);
		if (!values)
			return std::unexpected(values.error());
		std::string error;
		auto prepared = a_access.prepare(*values, error);
		if (!prepared)
			return std::unexpected(error);
		prepared->swap();
		return std::monostate{};
	}

	void TestLiveRoundTrip()
	{
		TestSettings settings;
		auto access = BindLiveSettings(kSchema, settings);

		// JSON integers must satisfy float fields, and arrays must reach array fields.
		const auto applied = Apply(access, json{ { "count", 4 }, { "thickness", 1 }, { "label", "b" }, { "tint", { 0, 1, 0.5 } } });
		CHECK(applied);
		CHECK(settings.count == 4);
		CHECK(settings.thickness == 1.0f);
		CHECK(settings.label == "b");
		CHECK(settings.tint == (Float3{ 0.0f, 1.0f, 0.5f }));

		const auto echoed = TomlToJson(access.snapshot());
		CHECK(echoed.at("count") == 4);
		CHECK(echoed.at("label") == "b");
		CHECK(echoed.at("tint") == json::array({ 0.0, 1.0, 0.5 }));
		CHECK(!echoed.contains("boot"));
	}

	void TestRejectionsLeaveStateUntouched()
	{
		TestSettings settings;
		auto access = BindLiveSettings(kSchema, settings);

		CHECK(!Apply(access, json{ { "count", 9 } }));
		CHECK(!Apply(access, json{ { "count", "two" } }));
		CHECK(!Apply(access, json{ { "tint", { 1, 2 } } }));
		CHECK(!Apply(access, json{ { "label", nullptr } }));
		CHECK(!Apply(access, json{ { "count", 18446744073709551615ull } }));
		CHECK(settings.count == 2);
		CHECK(settings.label == "a");
	}

	void TestKeyClassification()
	{
		TestSettings settings;
		auto access = BindLiveSettings(kSchema, settings);
		const auto values = JsonToToml(json{ { "count", 3 }, { "boot", 2 }, { "typo", true } });
		CHECK(values);
		const auto issues = ClassifyKeys(*values, access.snapshot(), MakeSchemaView(kSchema));
		CHECK(issues.restartOnly == std::vector<std::string>{ "boot" });
		CHECK(issues.unknown == std::vector<std::string>{ "typo" });
	}

	void TestNotComparableKeysStayLive()
	{
		TestSettings settings;
		auto access = BindLiveSettings(kSchema, settings, {}, { "label" });
		CHECK(access.snapshot().contains("label"));
		CHECK(Apply(access, json{ { "label", "c" } }));
		CHECK(settings.label == "c");
		const auto values = JsonToToml(json{ { "label", "d" } });
		CHECK(values && ClassifyKeys(*values, access.snapshot(), MakeSchemaView(kSchema)).empty());
	}
}

int main()
{
	TestLiveRoundTrip();
	TestRejectionsLeaveStateUntouched();
	TestKeyClassification();
	TestNotComparableKeysStayLive();
	if (cs::tests::failures)
		return 1;
	std::cout << "Settings JSON tests passed\n";
	return 0;
}
