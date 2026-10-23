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
	float CascadeSelect(const DirectionalShadowLightData<2>& a_data, float a_depth)
	{
		const float t = (a_depth - a_data.StartSplitDistances.y) /
		                (a_data.EndSplitDistances.x - a_data.StartSplitDistances.y);
		return std::fmin(std::fmax(t, 0.0f), 1.0f);
	}

	// Mirrors UpdateProbesCS cascade selection; N means lit past the last end.
	template <std::size_t N, typename Splits>
	std::size_t ProbeCascade(const Splits& a_end, float a_depth)
	{
		const float* ends = &a_end.x;
		if (a_depth >= ends[N - 1])
			return N;
		std::size_t index = 0;
		for (std::size_t i = 0; i + 1 < N; ++i)
			index += a_depth > ends[i] ? 1 : 0;
		return index;
	}

	void TestTwoCascadesBlendAroundSplit()
	{
		DirectionalShadowLightData<2> data;
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
		DirectionalShadowLightData<2> data;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(2, 0.0f), data));
		CHECK(data.EndSplitDistances.x > data.StartSplitDistances.y);
		CHECK(std::isfinite(CascadeSelect(data, 800.0f)));
	}

	void TestSingleCascadeNeverSelectsMissingSlice()
	{
		DirectionalShadowLightData<2> two;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(1, 512.0f), two));
		CHECK(two.EndSplitDistances.y == 1312.0f);
		CHECK(CascadeSelect(two, 1311.0f) == 0.0f);
		CHECK(std::memcmp(&two.ShadowProj[0], &two.ShadowProj[1], sizeof(two.ShadowProj[0])) == 0);

		DirectionalShadowLightData<3> three;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(1, 512.0f), three));
		CHECK(ProbeCascade<3>(three.EndSplitDistances, 1311.0f) == 0);
		CHECK(ProbeCascade<3>(three.EndSplitDistances, 1312.0f) == 3);
	}

	void TestTwoLiveCascadesNeverSelectSlotTwo()
	{
		DirectionalShadowLightData<3> data;
		CHECK(BuildDirectionalShadowLightData(MakeSnapshot(2, 512.0f), data));
		CHECK(data.EndSplitDistances.x == 1312.0f);
		CHECK(data.EndSplitDistances.y == 6656.0f);
		CHECK(data.EndSplitDistances.z == 6656.0f);
		CHECK(data.EndSplitDistances.w == 6656.0f);
		CHECK(ProbeCascade<3>(data.EndSplitDistances, 2000.0f) == 1);
		CHECK(ProbeCascade<3>(data.EndSplitDistances, 6655.0f) == 1);
		CHECK(ProbeCascade<3>(data.EndSplitDistances, 6656.0f) == 3);
	}

	void TestSingularMatrixIsRejected()
	{
		auto snapshot = MakeSnapshot(2, 512.0f);
		snapshot.worldToShadow[1] = {};
		DirectionalShadowLightData<2> two;
		DirectionalShadowLightData<3> three;
		CHECK(!BuildDirectionalShadowLightData(snapshot, two));
		CHECK(!BuildDirectionalShadowLightData(snapshot, three));
	}
}

int main()
{
	TestTwoCascadesBlendAroundSplit();
	TestZeroOverlapKeepsBandFinite();
	TestSingleCascadeNeverSelectsMissingSlice();
	TestTwoLiveCascadesNeverSelectSlotTwo();
	TestSingularMatrixIsRejected();
	return failures == 0 ? 0 : 1;
}
