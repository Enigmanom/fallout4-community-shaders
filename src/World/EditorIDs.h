#pragma once

#include <string_view>

namespace RE
{
	class TESForm;
}

namespace cs::engine
{
	// FO4 keeps no editor IDs for most forms; tracked types are cached as they load.
	void InstallEditorIDCache();

	// Empty for untracked or unnamed forms.
	std::string_view GetEditorID(const RE::TESForm* a_form) noexcept;
}
