#pragma once

#include "Descriptor.h"

#include <string>
#include <vector>

// Turns the parsed descriptor tree into the TAB STRUCTURE the settings page is drawn from.
//
// This layer exists so the tab rules are decided in code that can be proved without launching the
// game: the ImGui renderer becomes a thin walk over what is built here. The standing house rule is
// that a settings surface is broken into tabs by logical area and never presented as one long
// scrolling page, with a second level where a single area is itself large.
//
// Nothing here draws. Every pointer borrows from the Catalog, which is never mutated after load.
namespace SettingsPage
{
	// A tab, possibly carrying tabs of its own.
	struct Tab
	{
		std::string label;
		std::string desc;            // group help text, shown under the tab header when present
		const Entry* source{ nullptr };  // the group this tab came from; null for a synthetic tab

		// Controls shown directly on this tab, in descriptor order. Groups that became child tabs
		// are NOT in here; groups small enough to stay inline are flattened into it, keeping their
		// heading as a separator so the descriptor's own structure survives.
		std::vector<const Entry*> controls;

		std::vector<Tab> children;

		bool HasChildren() const { return !children.empty(); }

		// Only the entries that are actually settings. `controls` deliberately also carries the
		// group headings and static text labels of any group small enough to be flattened inline,
		// so its raw size overstates how much the player can change on this tab.
		int DirectControlCount() const
		{
			int count = 0;
			for (const auto* entry : controls) {
				if (IsControl(entry->type)) {
					count += 1;
				}
			}
			return count;
		}
	};

	// One descriptor file: the outer tab.
	struct PanelTab
	{
		const Panel* panel{ nullptr };
		std::string label;
		std::vector<Tab> tabs;
	};

	// Tuning for the split decisions. Defaults chosen from the measured census rather than taste:
	// the largest descriptor (Ammo Wheel, 475 nodes) has to break into a second level, while the
	// smallest (the 23-node layout file) must not sprout tabs it does not need.
	struct LayoutRules
	{
		// A group with more than this many controls beneath it earns its own tab rather than being
		// flattened inline into its parent tab.
		int promoteGroupToTabAbove{ 12 };
		// A tab holding more than this many direct controls tries to push its subgroups down into
		// child tabs instead of showing everything at once.
		int splitTabAbove{ 28 };
	};

	class PageModel
	{
	public:
		static PageModel& GetSingleton();

		// Builds the tab tree from an already-loaded catalogue. Safe to call again; rebuilds.
		void Build(const Catalog& a_catalog, const LayoutRules& a_rules = {});

		const std::vector<PanelTab>& Panels() const { return _panels; }
		bool IsBuilt() const { return _built; }
		const LayoutRules& Rules() const { return _rules; }

		struct Stats
		{
			int panels{ 0 };
			int topLevelTabs{ 0 };    // across all panels
			int totalTabs{ 0 };       // including nested
			int maxTabDepth{ 0 };
			int controlsPlaced{ 0 };  // must equal the catalogue's control count
			int maxControlsOnOneTab{ 0 };
			std::string busiestTab;
		};

		Stats Measure() const;
		void LogStructure() const;

	private:
		PageModel() = default;

		std::vector<PanelTab> _panels;
		LayoutRules _rules{};
		bool _built{ false };
	};
}
