#pragma once

#include <cmath>
#include <cstdint>

namespace cs::engine
{
	enum class RenderUIOutputExtent : std::uint8_t
	{
		kInvalid,
		kCommitted,
		kFull
	};

	[[nodiscard]] inline RenderUIOutputExtent ClassifyRenderUIOutputExtent(
		float a_topLeftX,
		float a_topLeftY,
		float a_width,
		float a_height,
		std::uint32_t a_committedWidth,
		std::uint32_t a_committedHeight,
		std::uint32_t a_fullWidth,
		std::uint32_t a_fullHeight) noexcept
	{
		const auto isNear = [](float a_value, std::uint32_t a_expected) {
			return std::isfinite(a_value) &&
			       std::abs(a_value - static_cast<float>(a_expected)) <= 0.5f;
		};
		if (!isNear(a_topLeftX, 0) || !isNear(a_topLeftY, 0) ||
			!a_committedWidth || !a_committedHeight ||
			!a_fullWidth || !a_fullHeight) {
			return RenderUIOutputExtent::kInvalid;
		}
		if (isNear(a_width, a_fullWidth) && isNear(a_height, a_fullHeight)) {
			return RenderUIOutputExtent::kFull;
		}
		if (isNear(a_width, a_committedWidth) &&
			isNear(a_height, a_committedHeight)) {
			return RenderUIOutputExtent::kCommitted;
		}
		return RenderUIOutputExtent::kInvalid;
	}

}
