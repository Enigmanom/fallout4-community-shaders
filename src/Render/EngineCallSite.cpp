#include "Render/EngineCallSite.h"

#include "PCH.h"

#include <cstring>
#include <format>
#include <utility>

namespace cs::engine
{
	namespace
	{
		std::string RuntimeLabel()
		{
			return std::format(
				"Fallout 4 {}", REX::FModule::GetExecutingModule().GetFileVersion());
		}

		bool InsideText(std::uintptr_t a_site, std::size_t a_size) noexcept
		{
			const auto text = REX::FModule::GetExecutingModule().GetSection(".text");
			const auto begin = text.GetAddress();
			return a_site >= begin && a_site + a_size <= begin + text.GetSize();
		}

		std::uintptr_t RuntimeSite(const REL::VariantID& a_function, const RuntimeOffsets& a_offset)
		{
			return a_function.address() +
			       a_offset[static_cast<std::size_t>(REX::FModule::GetRuntimeIndex())];
		}

		template <std::size_t N>
		struct SiteBytes
		{
			std::uintptr_t site;
			std::array<std::uint8_t, N> bytes;
		};

		template <std::size_t N, class Target>
		std::expected<SiteBytes<N>, std::string> ReadSite(const SiteAnchor<Target>& a_anchor) noexcept
		{
			SiteBytes<N> read{ RuntimeSite(a_anchor.function, a_anchor.offset), {} };
			if (!InsideText(read.site, N)) {
				return std::unexpected(std::format(
					"{}: call site {:#x} is outside the {} executable code",
					a_anchor.name, read.site, RuntimeLabel()));
			}
			std::memcpy(read.bytes.data(), reinterpret_cast<const void*>(read.site), N);
			return read;
		}
	}

	std::expected<std::uintptr_t, std::string> ResolveCallSite(
		const CallSiteAnchor& a_anchor) noexcept
	{
		const auto read = ReadSite<5>(a_anchor);
		if (!read)
			return std::unexpected(read.error());
		const auto [site, bytes] = *read;
		const auto expected = a_anchor.target.address();
		const auto classification = ClassifyCallSite(bytes, site, expected);
		switch (classification.kind) {
		case CallSiteKind::kExpected:
			return site;
		case CallSiteKind::kNotACall:
			return std::unexpected(std::format(
				"{}: {} has no rel32 call at {:#x} (bytes {:02X} {:02X} {:02X} {:02X} {:02X})",
				a_anchor.name, RuntimeLabel(), site,
				bytes[0], bytes[1], bytes[2], bytes[3], bytes[4]));
		case CallSiteKind::kWrongTarget:
			return std::unexpected(std::format(
				"{}: {} call at {:#x} targets {:#x}, expected {:#x}",
				a_anchor.name, RuntimeLabel(), site, classification.target, expected));
		}
		std::unreachable();
	}

	std::expected<std::uintptr_t, std::string> ResolveCallSite(
		const ContextCallSiteAnchor& a_anchor) noexcept
	{
		const auto read = ReadSite<6>(a_anchor);
		if (!read)
			return std::unexpected(read.error());
		const auto& [site, bytes] = *read;
		if (!IsContextVirtualCall(bytes, a_anchor.target.offset)) {
			return std::unexpected(std::format(
				"{}: {} has no context call [rax+{:#x}] at {:#x} (bytes {:02X} {:02X} {:02X} {:02X} {:02X} {:02X})",
				a_anchor.name, RuntimeLabel(), a_anchor.target.offset, site,
				bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5]));
		}
		return site;
	}

	std::expected<void, std::string> RedirectContextCall(
		std::uintptr_t a_site,
		std::uintptr_t a_thunk) noexcept
	{
		try {
			const REL::ASM::CALL5 call(a_site, REL::GetTrampoline().allocate_branch5(a_thunk));
			std::array<std::uint8_t, 6> patch{};
			std::memcpy(patch.data(), std::addressof(call), sizeof(call));
			patch[5] = REL::NOP;
			REL::WriteSafe(a_site, patch.data(), patch.size());
			FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<const void*>(a_site), patch.size());
		} catch (const std::exception& e) {
			return std::unexpected(std::format("context call redirect at {:#x}: {}", a_site, e.what()));
		}
		return {};
	}
}
