#include "Utils/WinApi.h"

namespace Util
{
	// FO4: port only topology; upstream's unrelated version reader assumes a Unicode PCH.
	uint32_t GetPerformanceCoreCount()
	{
		// Cache the result — CPU topology never changes at runtime.
		// C++11 guarantees thread-safe initialisation of static locals.
		static const uint32_t cached = []() -> uint32_t {
			const uint32_t fallback = std::max(1u, std::thread::hardware_concurrency());
			DWORD size = 0;
			GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &size);
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0)
				return fallback;
			std::vector<uint8_t> buf(size);
			auto* info = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data());
			if (!GetLogicalProcessorInformationEx(RelationProcessorCore, info, &size))
				return fallback;
			// First pass: find the highest efficiency class present.
			BYTE maxClass = 0;
			for (DWORD offset = 0; offset < size;) {
				auto* entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data() + offset);
				if (entry->Processor.EfficiencyClass > maxClass)
					maxClass = entry->Processor.EfficiencyClass;
				offset += entry->Size;
			}
			// Second pass: count logical processors on those (P-)cores.
			uint32_t count = 0;
			for (DWORD offset = 0; offset < size;) {
				auto* entry = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data() + offset);
				if (entry->Processor.EfficiencyClass == maxClass) {
					for (WORD g = 0; g < entry->Processor.GroupCount; ++g)
						count += static_cast<uint32_t>(std::popcount(entry->Processor.GroupMask[g].Mask));
				}
				offset += entry->Size;
			}
			return count > 0 ? count : fallback;
		}();
		return cached;
	}
}
