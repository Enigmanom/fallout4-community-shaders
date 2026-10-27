#pragma once

#include <DearModdingUI/UI.h>

#include <algorithm>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace Util
{
	/** @brief Row comparator for one column of a sortable table: (a, b, ascending) -> a before b. */
	template <typename T>
	using TableRowSortFunc = std::function<bool(const T&, const T&, bool)>;

	/** @brief The column a sortable table is sorted by and the direction. column -1 = leave the rows as they are. */
	struct TableSortSpec
	{
		int column = -1;
		bool ascending = true;
	};

	/** @brief One header of a sortable table. */
	struct TableSortColumn
	{
		const char* label;
		dmui::ui::TableColumnFlags flags = dmui::ui::TableColumnFlags::kNone;
		float widthOrWeight = 0.0f;
	};

	/**
	 * @brief Sets up and draws the current table's headers and returns its sort (between BeginTable and EndTable).
	 * a_state holds the sort across frames; a tristate table returns to column -1 on the third click.
	 */
	// FO4: replaces ImGui::TableHeadersRow + ReadTableSortSpec, as DmUI cannot read sort specs back.
	inline TableSortSpec TableSortHeaders(std::span<const TableSortColumn> a_columns, TableSortSpec& a_state, bool a_tristate = false)
	{
		// TODO(dmui 2.2): TableGetSortSpecs
		using dmui::ui::TableColumnFlags;
		for (const auto& column : a_columns)
			dmui::ui::TableSetupColumn(column.label, column.flags, column.widthOrWeight);
		dmui::ui::TableNextRow(dmui::ui::TableRowFlags::kHeaders);
		for (std::size_t i = 0; i < a_columns.size(); ++i) {
			(void)dmui::ui::TableSetColumnIndex(static_cast<int>(i));
			const bool sorted = a_state.column == static_cast<int>(i);
			const auto label = std::string(a_columns[i].label) + (sorted ? (a_state.ascending ? " ^" : " v") : "") + "##sort";
			if (!dmui::ui::Selectable(label.c_str(), sorted))
				continue;
			const bool preferAscending = (a_columns[i].flags & TableColumnFlags::kPreferSortDescending) == TableColumnFlags::kNone;
			if (!sorted)
				a_state = { static_cast<int>(i), preferAscending };
			else if (a_tristate && a_state.ascending != preferAscending)
				a_state = {};
			else
				a_state.ascending = !a_state.ascending;
		}
		return a_state;
	}

	/** @brief Stable-sorts rows with one comparator, unless the spec says to leave them as they are. */
	template <typename T>
	void SortTableRowsWith(std::vector<T>& rows, const TableSortSpec& spec, const TableRowSortFunc<T>& comparator)
	{
		if (spec.column < 0 || !comparator)
			return;
		const bool ascending = spec.ascending;
		std::stable_sort(rows.begin(), rows.end(), [&comparator, ascending](const T& a, const T& b) {
			return comparator(a, b, ascending);
		});
	}

	/** @brief Stable-sorts rows by the spec's column with that column's comparator; a column without one leaves the rows as they are. */
	template <typename T>
	void SortTableRows(std::vector<T>& rows, const TableSortSpec& spec, const std::vector<TableRowSortFunc<T>>& customSorts)
	{
		if (spec.column < 0 || static_cast<size_t>(spec.column) >= customSorts.size())
			return;
		SortTableRowsWith(rows, spec, customSorts[spec.column]);
	}
}
