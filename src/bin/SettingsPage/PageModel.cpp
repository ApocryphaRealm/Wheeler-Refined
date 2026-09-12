#include "PageModel.h"

#include <algorithm>

namespace SettingsPage
{
	namespace
	{
		// Controls beneath an entry, counting its whole subtree. Used for the split decisions.
		int CountControlsDeep(const Entry& a_entry)
		{
			int total = IsControl(a_entry.type) ? 1 : 0;
			for (const auto& child : a_entry.entries) {
				total += CountControlsDeep(child);
			}
			return total;
		}

		bool HasChildGroup(const Entry& a_entry)
		{
			return std::any_of(a_entry.entries.begin(), a_entry.entries.end(), [](const Entry& child) {
				return child.type == EntryType::Group;
			});
		}

		// Flattens an entry's subtree into a control list, in descriptor order. Static text labels
		// are kept: they are the descriptor's own headings and captions, and dropping them would
		// lose the grouping the author wrote.
		void FlattenInto(const Entry& a_entry, std::vector<const Entry*>& a_out)
		{
			for (const auto& child : a_entry.entries) {
				if (child.type == EntryType::Group) {
					// An inline group keeps its heading, then its contents.
					a_out.push_back(&child);
					FlattenInto(child, a_out);
				} else {
					a_out.push_back(&child);
				}
			}
		}

		Tab BuildTab(const Entry& a_group, const LayoutRules& a_rules);

		// Decides, for one group, which of its child groups become child tabs and which are
		// flattened inline. A child group is promoted when it is big enough to deserve a tab, or
		// when the parent would otherwise be an overlong page.
		void PopulateTab(Tab& a_tab, const Entry& a_group, const LayoutRules& a_rules)
		{
			const int directControls = CountControlsDeep(a_group);
			const bool parentIsCrowded = directControls > a_rules.splitTabAbove;

			for (const auto& child : a_group.entries) {
				if (child.type != EntryType::Group) {
					a_tab.controls.push_back(&child);
					continue;
				}

				const int childControls = CountControlsDeep(child);
				const bool promote =
					childControls > a_rules.promoteGroupToTabAbove ||
					(parentIsCrowded && childControls > 0);

				if (promote) {
					a_tab.children.push_back(BuildTab(child, a_rules));
				} else {
					// Small enough to sit on the parent tab: keep the heading, then the contents.
					a_tab.controls.push_back(&child);
					FlattenInto(child, a_tab.controls);
				}
			}
		}

		Tab BuildTab(const Entry& a_group, const LayoutRules& a_rules)
		{
			Tab tab{};
			tab.source = &a_group;
			tab.label = a_group.name.empty() ? std::string("Settings") : a_group.name;
			tab.desc = a_group.desc;
			PopulateTab(tab, a_group, a_rules);
			return tab;
		}

		void MeasureTab(const Tab& a_tab, PageModel::Stats& a_stats, int a_depth)
		{
			a_stats.totalTabs += 1;
			a_stats.maxTabDepth = (std::max)(a_stats.maxTabDepth, a_depth);

			int controlsHere = 0;
			for (const auto* entry : a_tab.controls) {
				if (IsControl(entry->type)) {
					controlsHere += 1;
				}
			}
			a_stats.controlsPlaced += controlsHere;
			if (controlsHere > a_stats.maxControlsOnOneTab) {
				a_stats.maxControlsOnOneTab = controlsHere;
				a_stats.busiestTab = a_tab.label;
			}

			for (const auto& child : a_tab.children) {
				MeasureTab(child, a_stats, a_depth + 1);
			}
		}
	}

	PageModel& PageModel::GetSingleton()
	{
		static PageModel singleton;
		return singleton;
	}

	void PageModel::Build(const Catalog& a_catalog, const LayoutRules& a_rules)
	{
		_panels.clear();
		_rules = a_rules;
		_built = false;

		for (const auto& panel : a_catalog.Panels()) {
			PanelTab panelTab{};
			panelTab.panel = &panel;
			panelTab.label = panel.name;

			// How many top-level groups the descriptor has decides the shape. Six of the eight
			// descriptors have several, and those map one group to one tab. Two of them
			// ("Wheel Behavior", 143 nodes, and "Wheeler I4", 45) have exactly ONE top-level group,
			// so taking it literally would produce a single tab holding everything - precisely the
			// long page tabs exist to prevent. For those, the lone group's own children become the
			// tabs instead.
			std::vector<const Entry*> topGroups;
			std::vector<const Entry*> looseEntries;
			for (const auto& entry : panel.entries) {
				if (entry.type == EntryType::Group) {
					topGroups.push_back(&entry);
				} else {
					looseEntries.push_back(&entry);
				}
			}

			if (topGroups.size() == 1 && HasChildGroup(*topGroups.front())) {
				const Entry& lone = *topGroups.front();
				for (const auto& child : lone.entries) {
					if (child.type == EntryType::Group) {
						panelTab.tabs.push_back(BuildTab(child, _rules));
					} else {
						// A control sitting beside the subgroups still needs a home; it goes on a
						// leading tab named after the group it actually belongs to.
						if (panelTab.tabs.empty() || panelTab.tabs.front().source != &lone) {
							Tab general{};
							general.source = &lone;
							general.label = lone.name.empty() ? std::string("General") : lone.name;
							general.desc = lone.desc;
							panelTab.tabs.insert(panelTab.tabs.begin(), std::move(general));
						}
						panelTab.tabs.front().controls.push_back(&child);
					}
				}
			} else {
				for (const auto* group : topGroups) {
					panelTab.tabs.push_back(BuildTab(*group, _rules));
				}
			}

			// Entries at the very top of the descriptor that are not in any group.
			if (!looseEntries.empty()) {
				Tab general{};
				general.label = "General";
				general.controls = looseEntries;
				panelTab.tabs.insert(panelTab.tabs.begin(), std::move(general));
			}

			// A tab with nothing the player can change is a label that opens onto nothing. It
			// happens when a descriptor's loose top-level entries are static TEXT - the panel's
			// own introduction - rather than settings, which is exactly the case in both Action
			// Hotkeys Bridge files. In one of them it also produced an empty "General" sitting
			// beside a real group of the same name, which reads as a duplicate.
			//
			// Fold such a tab into its neighbour rather than dropping it: the text is the
			// descriptor author's own heading material and belongs at the top of the first real
			// tab. Done as a general post-pass because the single-top-level-group branch can
			// produce the same shape - today's descriptors just do not happen to trigger it.
			for (std::size_t i = 0; i < panelTab.tabs.size();) {
				Tab& tab = panelTab.tabs[i];
				const bool nothingToShow = tab.DirectControlCount() == 0 && tab.children.empty();
				if (!nothingToShow || panelTab.tabs.size() == 1) {
					++i;
					continue;
				}

				if (i + 1 < panelTab.tabs.size()) {
					auto& into = panelTab.tabs[i + 1];
					into.controls.insert(into.controls.begin(), tab.controls.begin(), tab.controls.end());
				} else {
					auto& into = panelTab.tabs[i - 1];
					into.controls.insert(into.controls.end(), tab.controls.begin(), tab.controls.end());
				}
				panelTab.tabs.erase(panelTab.tabs.begin() + static_cast<std::ptrdiff_t>(i));
			}

			_panels.push_back(std::move(panelTab));
		}

		_built = !_panels.empty();
	}

	PageModel::Stats PageModel::Measure() const
	{
		Stats stats{};
		stats.panels = static_cast<int>(_panels.size());
		for (const auto& panel : _panels) {
			stats.topLevelTabs += static_cast<int>(panel.tabs.size());
			for (const auto& tab : panel.tabs) {
				MeasureTab(tab, stats, 0);
			}
		}
		return stats;
	}

	void PageModel::LogStructure() const
	{
		const auto stats = Measure();
		INFO("[SettingsPage] Page model: {} panels, {} top-level tabs, {} tabs total, depth {}, {} controls placed, busiest tab '{}' with {}",
			stats.panels, stats.topLevelTabs, stats.totalTabs, stats.maxTabDepth,
			stats.controlsPlaced, stats.busiestTab, stats.maxControlsOnOneTab);

		for (const auto& panel : _panels) {
			INFO("[SettingsPage]   {} - {} tabs", panel.label, panel.tabs.size());
		}
	}
}
