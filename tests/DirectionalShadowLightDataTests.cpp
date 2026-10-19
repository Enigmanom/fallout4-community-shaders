#include "Render/DirectionalShadowLightData.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <string_view>

namespace
{
	using namespace cs::engine;

	int failures{};

	void Check(bool a_condition, std::string_view a_expression, int a_line)
	{
		if (!a_condition) {
			std::cerr << "CHECK failed at line " << a_line << ": " << a_expression << '\n';
			++failures;
		}
	}

#define CHECK(a_expression) Check(static_cast<bool>(a_expression), #a_expression, __LINE__)

	SunCascadeSnapshot MakeSnapshot(std::uint32_t a_count, float a_overlap)
	{
		SunCascadeSnapshot snapshot;
		snapshot.count = a_count;
		snapshot.overlap = a_overlap;
		snapshot.splitEnd = { 800.0f, 6144.0f, 20000.0f };
		for (auto& matrix : snapshot.worldToShadow)
			DirectX::XMStoreFloat4x4(&matrix, DirectX::XMMatrixScaling(2.0f, 2.0f, 2.0f));
		return snapshot;
	}

	// Mirrors the upstream shader's blend factor.
	float CascadeSelect(const DirectionalShadowLightData& a_data, float a_depth)
	{
		const float t = (a_depth - a_data.StartSplitDistances.y) /
		                (a_data.EndSplitDistances.x - a_data.StartSplitDistances.y);
		return std::fmin(std::fmax(t, 0.0f), 1.0f);
	}

	void TestTwoCascadesBlendAroundSplit()
	{
		DirectionalShadowLightData data;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(2, 512.0f), data));
		CHECK(data.StartSplitDistances.y == 288.0f);
		CHECK(data.EndSplitDistances.x == 1312.0f);
		CHECK(data.EndSplitDistances.y == 6656.0f);
		CHECK(CascadeSelect(data, 100.0f) == 0.0f);
		CHECK(CascadeSelect(data, 800.0f) == 0.5f);
		CHECK(CascadeSelect(data, 3000.0f) == 1.0f);
	}

	void TestZeroOverlapKeepsBandFinite()
	{
		DirectionalShadowLightData data;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(2, 0.0f), data));
		CHECK(data.EndSplitDistances.x > data.StartSplitDistances.y);
		CHECK(std::isfinite(CascadeSelect(data, 800.0f)));
	}

	void TestSingleCascadeNeverSelectsSecond()
	{
		DirectionalShadowLightData data;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(1, 512.0f), data));
		CHECK(data.EndSplitDistances.y == 1312.0f);
		CHECK(CascadeSelect(data, 1311.0f) == 0.0f);
		CHECK(std::memcmp(&data.ShadowProj[0], &data.ShadowProj[1], sizeof(data.ShadowProj[0])) == 0);
	}

	void TestThirdCascadePublishesAsTwo()
	{
		DirectionalShadowLightData three;
		DirectionalShadowLightData two;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(3, 512.0f), three));
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(2, 512.0f), two));
		CHECK(std::memcmp(&three, &two, sizeof(three)) == 0);
	}

	void TestSingularMatrixIsRejected()
	{
		auto snapshot = MakeSnapshot(2, 512.0f);
		snapshot.worldToShadow[1] = {};
		DirectionalShadowLightData data;
		CHECK(!BuildDirectionalShadowLightData(snapshot, data));
	}
}

int main()
{
	TestTwoCascadesBlendAroundSplit();
	TestZeroOverlapKeepsBandFinite();
	TestSingleCascadeNeverSelectsSecond();
	TestThirdCascadePublishesAsTwo();
	TestSingularMatrixIsRejected();
	return failures == 0 ? 0 : 1;
}
