#include "World/EditorIDs.h"

#include "RE/T/TESWeather.h"

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace cs::engine
{
	namespace
	{
		struct Cache
		{
			std::shared_mutex mutex;
			std::unordered_map<const RE::TESForm*, std::string> ids;
		};

		Cache& GetCache()
		{
			static Cache cache;
			return cache;
		}

		template <class Form>
		struct SetFormEditorID
		{
			static bool thunk(Form* a_this, const char* a_editorID)
			{
				if (a_editorID && *a_editorID) {
					auto& cache = GetCache();
					const std::unique_lock lock(cache.mutex);
					cache.ids.insert_or_assign(a_this, a_editorID);
				}
				return func(a_this, a_editorID);
			}
			static inline REL::Relocation<decltype(thunk)> func;
			static constexpr std::size_t size = 0x3B;
		};
	}

	void InstallEditorIDCache()
	{
		stl::write_vfunc<RE::TESWeather, SetFormEditorID<RE::TESWeather>>();
	}

	std::string_view GetEditorID(const RE::TESForm* a_form) noexcept
	{
		if (!a_form)
			return {};
		auto& cache = GetCache();
		const std::shared_lock lock(cache.mutex);
		const auto found = cache.ids.find(a_form);
		// Entries are written only during data load, so the view outlives the lock.
		return found != cache.ids.end() ? std::string_view(found->second) : std::string_view{};
	}
}
