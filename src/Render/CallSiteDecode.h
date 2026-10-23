#pragma once

#include <cstdint>
#include <span>

namespace cs::engine
{
	enum class CallSiteKind : std::uint8_t
	{
		kExpected,
		kNotACall,
		kWrongTarget
	};

	struct CallSiteClassification
	{
		CallSiteKind kind = CallSiteKind::kNotACall;
		std::uintptr_t target = 0;
	};

	[[nodiscard]] constexpr CallSiteClassification ClassifyCallSite(
		std::span<const std::uint8_t, 5> a_bytes,
		std::uintptr_t a_site,
		std::uintptr_t a_expectedTarget) noexcept
	{
		if (a_bytes[0] != 0xE8) {
			return {};
		}
		const auto displacement = static_cast<std::int32_t>(
			static_cast<std::uint32_t>(a_bytes[1]) |
			(static_cast<std::uint32_t>(a_bytes[2]) << 8) |
			(static_cast<std::uint32_t>(a_bytes[3]) << 16) |
			(static_cast<std::uint32_t>(a_bytes[4]) << 24));
		const auto target = a_site + 5 + static_cast<std::intptr_t>(displacement);
		return { target == a_expectedTarget ? CallSiteKind::kExpected : CallSiteKind::kWrongTarget, target };
	}

	// `mov rax,[rcx]; call [rax+disp8]`: COM call on the object in rcx.
	[[nodiscard]] constexpr bool IsContextVirtualCall(
		std::span<const std::uint8_t, 6> a_bytes,
		std::uint8_t a_vtableOffset) noexcept
	{
		return a_bytes[0] == 0x48 && a_bytes[1] == 0x8B && a_bytes[2] == 0x01 &&
		       a_bytes[3] == 0xFF && a_bytes[4] == 0x50 && a_bytes[5] == a_vtableOffset;
	}
}