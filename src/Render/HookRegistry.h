#pragma once

#include "Render/RenderHooks.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace cs::engine
{
	// Entries run in priority order; equal priorities keep registration order.
	template <class Entry>
	void InsertByPriority(std::vector<Entry>& a_entries, Entry a_entry)
	{
		a_entries.push_back(std::move(a_entry));
		std::ranges::stable_sort(a_entries, {}, [](const Entry& a_item) { return static_cast<int>(a_item.priority); });
	}
}
