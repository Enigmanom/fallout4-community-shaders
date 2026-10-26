#include "Menu/Changelog.h"

#include "Log.h"
#include "Menu/ChangelogResource.h"
#include "Menu/Menu.h"

#include "Utils/ModuleResource.h"

namespace
{
	[[nodiscard]] cs::ChangelogHistory LoadChangelog()
	{
		const auto text = cs::LoadRcData(IDR_CHANGELOG);
		if (!text) {
			cs::ChangelogHistory result;
			result.error = text.error();
			return result;
		}
		return cs::ParseChangelogHistory(*text);
	}

	[[nodiscard]] const cs::ChangelogHistory& Changelog()
	{
		static const auto history = [] {
			auto result = LoadChangelog();
			if (!result.error.empty())
				cs::log::Get("cs.menu")->error("Changelog unavailable: {}", result.error);
			return result;
		}();
		return history;
	}
}

namespace cs
{
	void Menu::DrawChangelog(dmui::Client& a_client)
	{
		if (!CheckHostResult(
				a_client,
				a_client.DrawSectionHeader("Changelog"),
				"draw changelog section"))
			return;

		const auto& history = Changelog();
		if (!history.error.empty()) {
			dmui::ui::TextWrapped("Changelog unavailable. See FO4CommunityShaders.log.");
			return;
		}

		for (std::size_t index = 0; index < history.releases.size(); ++index) {
			const auto& release = history.releases[index];
			dmui::ui::PushID(release.version.c_str());
			const auto flags = index == 0 ?
			                       dmui::ui::TreeNodeFlags::kDefaultOpen :
			                       dmui::ui::TreeNodeFlags{};
			if (dmui::ui::CollapsingHeader(release.version.c_str(), flags)) {
				dmui::ui::PushTextWrapPos(0.0f);
				for (const auto& item : release.items) {
					CheckHostResult(
						a_client,
						a_client.DrawBulletText(item.c_str()),
						"draw changelog bullet");
				}
				dmui::ui::PopTextWrapPos();
			}
			dmui::ui::PopID();
		}
	}
}
