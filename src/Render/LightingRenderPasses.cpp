#include "Render/LightingRenderPasses.h"

#include "Log.h"

#include <algorithm>
#include <vector>

namespace
{
	auto* L = cs::log::Get("cs.hooks");
}

namespace cs::engine
{
	namespace
	{
		template <class Callback>
		struct Entry
		{
			HookPriority priority;
			Callback callback;
		};

		std::vector<Entry<PreLightingRenderPassesCallback>> g_pre;
		std::vector<Entry<PostLightingRenderPassesCallback>> g_post;
		bool g_installed = false;

		template <class Callback>
		void Insert(std::vector<Entry<Callback>>& a_entries, Callback a_callback, HookPriority a_priority)
		{
			a_entries.push_back({ a_priority, a_callback });
			std::ranges::stable_sort(a_entries, {}, [](const auto& a_entry) { return static_cast<int>(a_entry.priority); });
		}

		struct BSLightingShaderProperty_GetRenderPasses
		{
			static RE::BSShaderProperty::RenderPassArray* thunk(
				RE::BSLightingShaderProperty* a_property,
				RE::BSGeometry* a_geometry,
				std::uint32_t a_renderMode,
				RE::BSShaderAccumulator* a_accumulator)
			{
				const LightingRenderPassesCall call{ a_property, a_geometry, a_renderMode, a_accumulator };
				for (const auto& entry : g_pre)
					entry.callback(call);
				auto* passes = func(a_property, a_geometry, a_renderMode, a_accumulator);
				for (const auto& entry : g_post)
					entry.callback(call, passes);
				return passes;
			}
			static inline REL::Relocation<decltype(thunk)> func;
		};

		bool EnsureInstalled()
		{
			if (g_installed)
				return true;
			constexpr std::size_t kGetRenderPassesVtableSlot = 43;
			stl::write_vfunc<kGetRenderPassesVtableSlot, BSLightingShaderProperty_GetRenderPasses>(
				RE::VTABLE::BSLightingShaderProperty[0]);
			if (BSLightingShaderProperty_GetRenderPasses::func.address() == 0) {
				L->error("Hook failed on BSLightingShaderProperty::GetRenderPasses");
				return false;
			}
			g_installed = true;
			L->info("Hook installed on BSLightingShaderProperty::GetRenderPasses (vtable slot {})", kGetRenderPassesVtableSlot);
			return true;
		}
	}

	bool RegisterPreLightingRenderPasses(PreLightingRenderPassesCallback a_callback, HookPriority a_priority)
	{
		if (!RenderHookRegistrationAllowed("PreLightingRenderPasses") || !EnsureInstalled())
			return false;
		Insert(g_pre, a_callback, a_priority);
		return true;
	}

	bool RegisterPostLightingRenderPasses(PostLightingRenderPassesCallback a_callback, HookPriority a_priority)
	{
		if (!RenderHookRegistrationAllowed("PostLightingRenderPasses") || !EnsureInstalled())
			return false;
		Insert(g_post, a_callback, a_priority);
		return true;
	}
}
