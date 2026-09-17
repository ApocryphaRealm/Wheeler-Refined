#include "bin/AMF/AmfPage.h"
#include "bin/SettingsPresets.h"
#include "bin/AMF/AMFLaunch.h"

// The consumer header carries its own copies of ImGui's type and flag names inside namespace
// ImGuiMCP, so it coexists with the real imgui.h the precompiled header pulls in for Wheeler's
// own context. The one collision is the IM_COL32 macro, redefined identically: silenced here.
#pragma warning(push)
#pragma warning(disable : 4005)
#include "bin/AMF/SKSEMenuFramework.h"
#include "bin/AMF/Toggle.h"
#pragma warning(pop)

#include "bin/Config.h"
#include "bin/SettingsPage/Descriptor.h"
#include "bin/SettingsPage/PageLogic.h"
#include "bin/SettingsPage/PageModel.h"
#include "bin/SettingsPage/PageShared.h"
#include "bin/SettingsPage/ValueStore.h"
#include "bin/Texts.h"
#include "bin/UserInput/Controls.h"
#include "bin/Wheeler/Wheeler.h"

#include <array>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace AmfPage
{
	namespace
	{
		using namespace SettingsPage;
		namespace Shared = SettingsPage::Page::Shared;
		namespace MCP = ImGuiMCP;

		constexpr const char* kSectionName = "Perfected Wheeler";
		constexpr int kMaxPanels = 16;

		bool g_hosted = false;
		std::string g_hostedBy;

		// Panel index -> the model's panel tab, captured at Register(). The framework calls a plain
		// function pointer with no context, hence the trampolines below.
		std::vector<const PanelTab*> g_panels;

		// ---- helpers ------------------------------------------------------------------------------

		void HelpMarker(const Entry& a_entry)
		{
			if (a_entry.desc.empty()) {
				return;
			}
			MCP::SameLine();
			MCP::TextDisabled("(?)");
			if (MCP::IsItemHovered()) {
				MCP::SetItemTooltip("%s", a_entry.desc.c_str());
			}
		}

		void DrawCheckbox(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
		{
			bool value = a_current.AsBool(a_entry.defaultBool.value_or(false));
			// Rule 32: a boolean is an on/off SWITCH (the vendored Toggle draws it with the framework's primitives).
			if (MCP::Toggle(a_entry.name.c_str(), &value)) {
				if (ValueStore::GetSingleton().Set(a_panel, a_entry, FormatBoolLike(value, a_current))) {
					Shared::ApplyChangedSettings();
				}
			}
		}

		void DrawSlider(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
		{
			const double fallback = a_entry.defaultNumber.value_or(0.0);
			double value = a_current.AsNumber(fallback);
			double minimum = 0.0, maximum = 1.0, step = 0.0;
			if (a_entry.slider) {
				minimum = a_entry.slider->min;
				maximum = a_entry.slider->max;
				step = a_entry.slider->step;
			}
			const bool integral = (step >= 1.0) && (step == static_cast<double>(static_cast<long long>(step)));
			bool changed = false;
			if (integral) {
				int shown = static_cast<int>(value);
				changed = MCP::SliderInt(a_entry.name.c_str(), &shown, static_cast<int>(minimum), static_cast<int>(maximum));
				if (changed) {
					value = static_cast<double>(shown);
				}
			} else {
				float shown = static_cast<float>(value);
				changed = MCP::SliderFloat(a_entry.name.c_str(), &shown, static_cast<float>(minimum), static_cast<float>(maximum), "%.3f");
				if (changed) {
					value = static_cast<double>(shown);
				}
			}
			if (changed) {
				if (ValueStore::GetSingleton().Set(a_panel, a_entry, FormatNumberLike(value, a_current))) {
					Shared::ApplyChangedSettings();
				}
			}
		}

		void DrawColor(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
		{
			ColorValue colour = a_current.AsColor(a_entry.defaultColor.value_or(ColorValue{}));
			float rgba[4] = { colour.r, colour.g, colour.b, colour.a };
			const bool overridden = IsOverriddenByPreset(a_panel, a_entry, ValueStore::GetSingleton());
			if (overridden) {
				MCP::BeginDisabled(true);
			}
			if (MCP::ColorEdit4(a_entry.name.c_str(), rgba)) {
				colour.r = rgba[0];
				colour.g = rgba[1];
				colour.b = rgba[2];
				colour.a = rgba[3];
				if (ValueStore::GetSingleton().Set(a_panel, a_entry, FormatColorLike(colour, a_current))) {
					Shared::ApplyChangedSettings();
				}
			}
			if (overridden) {
				MCP::EndDisabled();
				MCP::SameLine();
				MCP::TextDisabled("(a preset is overriding this)");
				if (MCP::IsItemHovered()) {
					MCP::SetItemTooltip("%s is set to a palette preset, which replaces this colour on every load.\nSet that preset to Custom to use the colour chosen here.",
						PresetKeyFor(a_entry).c_str());
				}
			}
		}

		void DrawDropdown(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
		{
			if (a_entry.options.empty()) {
				return;
			}
			std::vector<const char*> items;
			items.reserve(a_entry.options.size());
			for (const auto& option : a_entry.options) {
				items.push_back(option.c_str());
			}
			int index = static_cast<int>(a_current.AsNumber(a_entry.defaultNumber.value_or(0.0)));
			index = (std::max)(0, (std::min)(index, static_cast<int>(items.size()) - 1));
			if (MCP::Combo(a_entry.name.c_str(), &index, items.data(), static_cast<int>(items.size()))) {
				if (ValueStore::GetSingleton().Set(a_panel, a_entry, FormatIndexLike(index, a_current))) {
					Shared::ApplyChangedSettings();
				}
			}
		}

		void DrawTextbox(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
		{
			std::array<char, 512> buffer{};
			const std::string current = a_current.AsString(a_entry.defaultString.value_or(std::string{}));
			std::strncpy(buffer.data(), current.c_str(), buffer.size() - 1);
			if (MCP::InputText(a_entry.name.c_str(), buffer.data(), buffer.size(), MCP::ImGuiInputTextFlags_EnterReturnsTrue)) {
				if (ValueStore::GetSingleton().Set(a_panel, a_entry, std::string(buffer.data()))) {
					Shared::ApplyChangedSettings();
				}
			}
		}

		void DrawButton(const Entry& a_entry)
		{
			if (!Shared::IsKnownButton(a_entry.id)) {
				MCP::BeginDisabled(true);
				MCP::Button(a_entry.name.c_str());
				MCP::EndDisabled();
				if (MCP::IsItemHovered()) {
					MCP::SetItemTooltip("No action is wired to this button%s.",
						a_entry.id.empty() ? "" : (" (id: " + a_entry.id + ")").c_str());
				}
				return;
			}
			if (MCP::Button(a_entry.name.c_str())) {
				Shared::RunButtonAction(a_entry.id);
			}
		}

		void DrawKeymap(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
		{
			std::uint32_t bound = 0;
			const auto outcome = Shared::ConsumeCapture(a_panel, a_entry, a_current, bound);

			const auto current = (outcome == Shared::CaptureOutcome::Bound) ?
				bound :
				static_cast<std::uint32_t>(a_current.AsNumber(a_entry.defaultNumber.value_or(0.0)));
			const bool gamepad = Shared::LooksLikeGamepad(a_entry);
			const std::string name = gamepad ? Controls::GetKeyNameForGamepad(current) : Controls::GetKeyNameForMkb(current);

			MCP::Text("%s", a_entry.name.c_str());
			MCP::SameLine();

			if (outcome == Shared::CaptureOutcome::Waiting) {
				MCP::TextDisabled("press a key...");
				MCP::SameLine();
				if (MCP::SmallButton("Cancel")) {
					Shared::CancelCapture();
				}
				return;
			}

			MCP::TextDisabled("%s", name.c_str());
			MCP::SameLine();
			// One row armed at a time: a second capture would steal the first one's press.
			MCP::BeginDisabled(Shared::IsAnyCapturing());
			if (MCP::SmallButton("Rebind")) {
				Shared::BeginCapture(Shared::CaptureId(a_entry));
			}
			MCP::EndDisabled();

			// Unbind beside Rebind, the same as the overlay's row (the owner, 2026-09-15). Disabled when
			// the row is already unbound.
			MCP::SameLine();
			MCP::BeginDisabled(Shared::IsAnyCapturing() || current == 0u);
			if (MCP::SmallButton(Texts::GetText(Texts::TextType::KeymapUnbindButton))) {
				Shared::Unbind(a_panel, a_entry, a_current);
			}
			MCP::EndDisabled();

			const std::string notice = Shared::RowNotice(Shared::CaptureId(a_entry));
			if (!notice.empty()) {
				MCP::SameLine();
				MCP::TextDisabled("%s", notice.c_str());
			} else if (!gamepad && AMFLaunch::IsKeyReservedByFramework(current)) {
				MCP::SameLine();
				MCP::TextDisabled("%s", Texts::GetText(Texts::TextType::KeymapReservedByFramework));
			}
		}

		void DrawEntry(const Panel& a_panel, const Entry& a_entry)
		{
			if (a_entry.type == EntryType::Group) {
				if (!a_entry.name.empty()) {
					MCP::SeparatorText(a_entry.name.c_str());
				}
				return;
			}
			if (a_entry.type == EntryType::Text) {
				if (!a_entry.name.empty()) {
					MCP::TextWrapped("%s", a_entry.name.c_str());
				}
				if (!a_entry.desc.empty()) {
					MCP::TextDisabled("%s", a_entry.desc.c_str());
				}
				return;
			}

			const auto visibility = EvaluateVisibility(Catalog::GetSingleton(), a_panel, a_entry, ValueStore::GetSingleton());
			if (visibility == Visibility::Hidden) {
				return;
			}
			const bool disabled = (visibility == Visibility::Disabled);
			if (disabled) {
				MCP::BeginDisabled(true);
			}
			MCP::PushID(a_entry.iniKey.empty() ? a_entry.name.c_str() : a_entry.iniKey.c_str());
			const auto current = ValueStore::GetSingleton().Get(a_panel, a_entry);
			switch (a_entry.type) {
			case EntryType::Checkbox: DrawCheckbox(a_panel, a_entry, current); break;
			case EntryType::Slider:   DrawSlider(a_panel, a_entry, current); break;
			case EntryType::Color:    DrawColor(a_panel, a_entry, current); break;
			case EntryType::Dropdown: DrawDropdown(a_panel, a_entry, current); break;
			case EntryType::Textbox:  DrawTextbox(a_panel, a_entry, current); break;
			case EntryType::Keymap:   DrawKeymap(a_panel, a_entry, current); break;
			case EntryType::Button:   DrawButton(a_entry); break;
			default: break;
			}
			HelpMarker(a_entry);
			MCP::PopID();
			if (disabled) {
				MCP::EndDisabled();
			}
		}

	// Settings presets (the owner, 2026-09-13): the shipped defaults and named user presets - global,
		// never tied to a save. Drawn at the top of Wheeler Controls / General on both
		// surfaces; the state (name box, notice, selection) lives in SettingsPresets so the two
		// pages cannot disagree about what is selected or what just happened.
		void DrawPresetsPanel()
		{
			MCP::PushID("settings-presets");
			MCP::SeparatorText(Texts::GetText(Texts::TextType::PresetsHeader));
			MCP::TextWrapped("%s", Texts::GetText(Texts::TextType::PresetsHelp));
			const auto names = SettingsPresets::List();
			const std::string current = SettingsPresets::Selected();
			std::vector<const char*> items;
			items.push_back(Texts::GetText(Texts::TextType::PresetNone));
			int index = 0;
			for (std::size_t i = 0; i < names.size(); ++i) {
				items.push_back(names[i].c_str());
				if (names[i] == current) {
					index = static_cast<int>(i) + 1;
				}
			}
			MCP::SetNextItemWidth(280.0f);
			if (MCP::Combo(Texts::GetText(Texts::TextType::PresetLabel), &index, items.data(), static_cast<int>(items.size()))) {
				std::string err;
				if (index <= 0) {
					SettingsPresets::ClearSelected();
					SettingsPresets::SetNotice(Texts::GetText(Texts::TextType::PresetNoneSelected));
				} else if (SettingsPresets::Load(names[index - 1], err)) {
					SettingsPresets::SetSelected(names[index - 1]);
					SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetLoaded)) + " " + names[index - 1]);
				} else {
					SettingsPresets::SetNotice(err);
				}
			}
			MCP::SetNextItemWidth(280.0f);
			MCP::InputText(Texts::GetText(Texts::TextType::PresetNameLabel), SettingsPresets::NameBuffer(), SettingsPresets::NameBufferSize(), MCP::ImGuiInputTextFlags_None);
			MCP::SameLine();
			if (MCP::Button(Texts::GetText(Texts::TextType::PresetSaveAs))) {
				std::string name = SettingsPresets::NameBuffer();
				if (name.find_first_not_of(' ') == std::string::npos) {
					name = current;   // nothing typed: overwrite the selected preset
				}
				std::string err;
				if (SettingsPresets::SaveAs(name, err)) {
					SettingsPresets::SetSelected(name);
					SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetSaved)) + " " + SettingsPresets::Selected());
				} else {
					SettingsPresets::SetNotice(err);
				}
			}
			const bool none = current.empty();
			if (none) {
				MCP::BeginDisabled(true);
			}
			if (MCP::Button(Texts::GetText(Texts::TextType::PresetRename))) {
				const std::string to = SettingsPresets::NameBuffer();
				std::string err;
				if (SettingsPresets::Rename(current, to, err)) {
					SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetRenamed)) + " " + SettingsPresets::Selected());
				} else {
					SettingsPresets::SetNotice(err);
				}
			}
			MCP::SameLine();
			if (MCP::Button(Texts::GetText(Texts::TextType::PresetDelete))) {
				std::string err;
				if (SettingsPresets::Delete(current, err)) {
					SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetDeleted)) + " " + current);
				} else {
					SettingsPresets::SetNotice(err);
				}
			}
			if (none) {
				MCP::EndDisabled();
			}
			MCP::SameLine();
			if (MCP::Button(Texts::GetText(Texts::TextType::PresetResetDefaults))) {
				std::string err;
				SettingsPresets::SetNotice(SettingsPresets::ResetToDefaults(err) ? Texts::GetText(Texts::TextType::PresetDefaultsRestored) : err.c_str());
			}
			MCP::SameLine();
			if (MCP::Button(Texts::GetText(Texts::TextType::PresetReloadIni))) {
				SettingsPresets::ReloadFromIni();
				SettingsPresets::SetNotice(Texts::GetText(Texts::TextType::PresetReloaded));
			}
			const std::string notice = SettingsPresets::Notice();
			if (!notice.empty()) {
				MCP::TextWrapped("%s", notice.c_str());
			}
			MCP::Separator();
			MCP::PopID();
		}

		// M7's live slider, on the Wheel Behavior panel: acts on the loaded wheel, not an INI value.
		void DrawLiveSlotCount()
		{
			const int live = Wheeler::GetCurrentWheelSlotCount();
			if (live < 0) {
				MCP::TextDisabled("%s", Texts::GetText(Texts::TextType::SlotCountNoWheel));
				MCP::SameLine();
				if (MCP::Button(Texts::GetText(Texts::TextType::SlotCountCreateFirst))) {
					Wheeler::PushWheel();
					logger::info("[AmfPage] first wheel created from the framework page");
				}
				MCP::Separator();
				return;
			}
			static int s_pending = 0;
			static bool s_dragging = false;
			if (!s_dragging) {
				s_pending = live;
			}
			MCP::SetNextItemWidth(220.0f);
			MCP::SliderInt(Texts::GetText(Texts::TextType::SlotCountLabel), &s_pending, 1, 64);
			s_dragging = MCP::IsItemActive();
			if (MCP::IsItemDeactivatedAfterEdit() && s_pending != live) {
				const int result = Wheeler::SetCurrentWheelSlotCount(s_pending);
				logger::info("[AmfPage] slot count {} -> requested {} -> now {}", live, s_pending, result);
				s_pending = result;
			}
			MCP::TextDisabled("%s", Texts::GetText(Texts::TextType::SlotCountHelp));
			MCP::Separator();
			static int s_pendingWheels = 0;
			static bool s_draggingWheels = false;
			const int liveWheels = Wheeler::GetWheelCount();
			if (!s_draggingWheels) {
				s_pendingWheels = liveWheels;
			}
			MCP::SetNextItemWidth(220.0f);
			MCP::SliderInt(Texts::GetText(Texts::TextType::WheelCountLabel), &s_pendingWheels, 1, 100);
			s_draggingWheels = MCP::IsItemActive();
			if (MCP::IsItemDeactivatedAfterEdit() && s_pendingWheels != liveWheels) {
				const int result = Wheeler::SetWheelCount(s_pendingWheels);
				logger::info("[AmfPage] wheel count {} -> requested {} -> now {}", liveWheels, s_pendingWheels, result);
				s_pendingWheels = result;
			}
			MCP::TextDisabled("%s", Texts::GetText(Texts::TextType::WheelCountHelp));
			MCP::Separator();
		}

		// Defined below, beside the chooser that owns them: a tab bar registers itself as it draws, and the
		// section publishes which bar a D-pad press belongs to once every bar has been seen.

		int DeclareInnerTabs(int a_count, int a_current);  // defined below, beside the export it resolves

		// WHICH tab bar a D-pad press should move.
		//
		// A section can draw a bar inside a bar - Wheeler Controls, then Input Bindings, then Mouse and Keyboard -
		// and only one of them can answer the press. Declaring the outermost made the player click a row before it
		// would move, and it still fell back to the top bar afterwards (the owner, 2026-09-16: "wherever the nav box
		// is should be considered to be active for navigation because otherwise you have to click every single row").
		//
		// So every bar registers itself as it draws, and the press goes to the one the NAV CURSOR is on. When the
		// cursor is on none of them the DEEPEST bar wins, which is the one the player opened their way into. Focus
		// is only known after a tab item has been submitted, so a bar is judged on what was seen LAST frame and the
		// request applied on the next - the same one-frame handover the framework uses for its own bar.
		struct InnerBar
		{
			std::string id;
			int count = 0;
			int index = 0;
			int depth = 0;
			bool focused = false;
		};

		std::vector<InnerBar> g_innerBars;      // registered this frame
		std::map<std::string, bool> s_subFocused;  // which nested bar had the cursor last frame
		std::string g_chosenBarId;              // the bar the press belongs to
		int g_pendingRequest = -1;              // the tab it was asked for, handed over once

		int RegisterInnerBar(const std::string& a_id, int a_count, int a_index, int a_depth, bool a_focused)
		{
			g_innerBars.push_back(InnerBar{ a_id, a_count, a_index, a_depth, a_focused });
			if (a_id != g_chosenBarId) {
				return -1;
			}
			const int request = g_pendingRequest;
			g_pendingRequest = -1;
			return (request >= 0 && request < a_count) ? request : -1;
		}

		void PublishChosenInnerBar()
		{
			const InnerBar* chosen = nullptr;
			for (const InnerBar& bar : g_innerBars) {
				if (bar.count <= 1) {
					continue;
				}
				if (bar.focused) {
					chosen = &bar;   // the nav cursor is on this bar: it owns the press
					break;
				}
				if (!chosen || bar.depth > chosen->depth) {
					chosen = &bar;   // otherwise the deepest bar drawn
				}
			}
			if (!chosen) {
				g_innerBars.clear();
				g_chosenBarId.clear();
				return;
			}
			g_chosenBarId = chosen->id;
			const int count = chosen->count;
			const int index = chosen->index;
			g_innerBars.clear();
			const int request = DeclareInnerTabs(count, index);
			if (request >= 0) {
				g_pendingRequest = request;
			}
		}


		void DrawTab(const Panel& a_panel, const Tab& a_tab, const std::string& a_id = std::string(), int a_depth = 0)
		{
			MCP::PushID(static_cast<const void*>(&a_tab));
			if (!a_tab.desc.empty()) {
				MCP::TextDisabled("%s", a_tab.desc.c_str());
				MCP::Separator();
			}
			for (const auto* entry : a_tab.controls) {
				if (entry) {
					DrawEntry(a_panel, *entry);
				}
			}
			if (!a_tab.children.empty()) {
				// This bar registers itself like any other, under an id built from its own tab's label, so a press
				// can be routed to it when the nav cursor is here rather than always to the section's top bar.
				const std::string subId = a_id + "/" + a_tab.label;
				static std::map<std::string, int> s_subIndex;
				const int subRequest = RegisterInnerBar(subId, static_cast<int>(a_tab.children.size()),
														s_subIndex[subId], a_depth + 1, s_subFocused[subId]);
				bool subFocused = false;
				if (MCP::BeginTabBar("sub", MCP::ImGuiTabBarFlags_FittingPolicyScroll | MCP::ImGuiTabBarFlags_TabListPopupButton)) {
					int childIdx = 0;
					for (const auto& child : a_tab.children) {
						const MCP::ImGuiTabItemFlags flags =
							(childIdx == subRequest) ? MCP::ImGuiTabItemFlags_SetSelected : 0;
						const bool open = MCP::BeginTabItem(child.label.c_str(), nullptr, flags);
						if (MCP::IsItemFocused()) {
							subFocused = true;   // the nav cursor is on this bar
						}
						if (open) {
							s_subIndex[subId] = childIdx;
							DrawTab(a_panel, child, subId, a_depth + 1);
							MCP::EndTabItem();
						}
						++childIdx;
					}
					MCP::EndTabBar();
				}
				s_subFocused[subId] = subFocused;
			}
			MCP::PopID();
		}

		// A captured key is consumed the frame it arrives, even when the armed row's own tab is not the
		// one showing (the player armed it, then the tab bar moved, or the row scrolled away). The
		// row that is showing consumes first through DrawKeymap; this catches the rest.
		void ConsumeArmedRowAnywhere()
		{
			if (!Shared::IsAnyCapturing()) {
				return;
			}
			for (const auto* panelTab : g_panels) {
				if (!panelTab || !panelTab->panel) {
					continue;
				}
				std::function<bool(const Entry&)> walk = [&](const Entry& entry) -> bool {
					for (const Entry& child : entry.entries) {
						if (walk(child)) {
							return true;
						}
					}
					if (entry.type == EntryType::Keymap && entry.HasIniTarget() && Shared::IsCapturing(Shared::CaptureId(entry))) {
						std::uint32_t bound = 0;
						const auto current = ValueStore::GetSingleton().Get(*panelTab->panel, entry);
						Shared::ConsumeCapture(*panelTab->panel, entry, current, bound);
						return true;
					}
					return false;
				};
				for (const Entry& entry : panelTab->panel->entries) {
					if (walk(entry)) {
						return;
					}
				}
			}
		}

		// Advanced settings off hides the sections after the first three from the framework's tabs (1.1.6; the
		// owner: "the advanced settings toggle doesnt hide the advanced settings tabs"). Apocrypha Menu Framework
		// 1.8.3+ exports AMF_SetPageVisible; the page names are the labels given to AddSectionItem under
		// kSectionName. Applied at registration and from whichever section is drawing, so a change made on
		// Wheeler Controls / General (or a preset load) takes effect on the next frame. A framework without the
		// export keeps the one-line notice RenderPanelIndex draws in each advanced section.
		// The framework's D-pad navigation can only see the tab bar IT submits, so the bar this page draws
		// itself is invisible to it and left/right did nothing inside a section (the owner, 2026-09-16: "the nav
		// box behaves properly on the main tabs of the mod page for Wheeler, but when going to the other tabs
		// within those tabs, it does not").
		//
		// AMF 1.8.7 exports AMF_DeclareInnerTabs(count, current) for exactly this: the page says what it has and
		// gets back the tab the D-pad asked for, or -1. A framework without the export simply returns nothing and
		// the bar behaves as it always did, so this is safe against an older AMF.
		int DeclareInnerTabs(int a_count, int a_current)
		{
			using DeclareFn = int (*)(int, int);
			static const DeclareFn declare = GetMenuFrameworkFunction<DeclareFn>("AMF_DeclareInnerTabs");
			if (!declare || a_count <= 1) {
				return -1;
			}
			return declare(a_count, a_current);
		}

		void SyncAdvancedVisibility()
		{
			using SetPageVisibleFn = bool (*)(const char*, const char*, bool);
			static const SetPageVisibleFn setVisible = GetMenuFrameworkFunction<SetPageVisibleFn>("AMF_SetPageVisible");
			if (!setVisible) {
				return;
			}
			static int applied = -1;
			const int want = Config::Control::Wheel::ShowAdvancedSettings ? 1 : 0;
			if (applied == want) {
				return;
			}
			int changed = 0;
			for (std::size_t i = 3; i < g_panels.size(); ++i) {
				if (g_panels[i] && setVisible(kSectionName, g_panels[i]->label.c_str(), want == 1)) {
					++changed;
				}
			}
			applied = want;
			logger::info("[AmfPage] advanced settings {}: {} section(s) {} in the framework's tabs", want ? "on" : "off", changed, want ? "shown" : "hidden");
		}

		void RenderPanelIndex(int a_index)
		{
			SyncAdvancedVisibility();
			if (a_index < 0 || a_index >= static_cast<int>(g_panels.size()) || !g_panels[a_index] || !g_panels[a_index]->panel) {
				MCP::TextDisabled("This panel is not available.");
				return;
			}
			const PanelTab& panelTab = *g_panels[a_index];
			// Advanced settings off (the owner, 2026-09-13): sections after the first three. With Apocrypha Menu
			// Framework 1.8.3+ they are hidden from the tabs (SyncAdvancedVisibility above) and never drawn; an older
			// framework or SKSE Menu Framework lists every section it was given, so they collapse to a line.
			if (a_index >= 3 && !Config::Control::Wheel::ShowAdvancedSettings) {
				MCP::TextWrapped("%s", Texts::GetText(Texts::TextType::AdvancedSettingsHidden));
				return;
			}
			MCP::PushID(a_index);
			ConsumeArmedRowAnywhere();
			if (panelTab.label.find("Wheel Behavior") != std::string::npos) {
				DrawLiveSlotCount();
			}
			// Which of this section's own tabs is open, remembered per section so the framework can be told it
			// at the START of the frame - the open tab is only known once the items have been submitted.
			static int s_innerIndex[16] = {};
			static bool s_innerFocused[16] = {};
			const int panelSlot = (a_index >= 0 && a_index < 16) ? a_index : 0;
			const int innerCount = static_cast<int>(panelTab.tabs.size());
			const std::string topId = "panel" + std::to_string(panelSlot);
			const int innerRequest = RegisterInnerBar(topId, innerCount, s_innerIndex[panelSlot], 0, s_innerFocused[panelSlot]);
			bool topFocused = false;
			if (MCP::BeginTabBar("tabs", MCP::ImGuiTabBarFlags_FittingPolicyScroll | MCP::ImGuiTabBarFlags_TabListPopupButton)) {
				int innerIdx = 0;
				for (const auto& tab : panelTab.tabs) {
					// The D-pad asks for its tab for exactly ONE frame; every other frame the bar owns its own
					// selection, so a press, a click and the tab-list popup never fight over what is open.
					const MCP::ImGuiTabItemFlags flags =
						(innerIdx == innerRequest) ? MCP::ImGuiTabItemFlags_SetSelected : 0;
					const bool topOpen = MCP::BeginTabItem(tab.label.c_str(), nullptr, flags);
					if (MCP::IsItemFocused()) {
						topFocused = true;
					}
					if (topOpen) {
						s_innerIndex[panelSlot] = innerIdx;
						if (panelTab.label.find("Wheeler Controls") != std::string::npos && &tab == &panelTab.tabs.front()) {
							DrawPresetsPanel();   // General tab, above the descriptor's own controls
						}
						DrawTab(*panelTab.panel, tab, topId, 0);
						MCP::EndTabItem();
					}
					++innerIdx;
				}
				MCP::EndTabBar();
			}
			s_innerFocused[panelSlot] = topFocused;
			// Every bar this section drew has registered by now: pick the one the press belongs to.
			PublishChosenInnerBar();
			MCP::PopID();
		}

		template <int N>
		void RenderPanel()
		{
			RenderPanelIndex(N);
		}

		using RenderFn = void (*)();
		constexpr std::array<RenderFn, kMaxPanels> kTrampolines = {
			&RenderPanel<0>, &RenderPanel<1>, &RenderPanel<2>, &RenderPanel<3>,
			&RenderPanel<4>, &RenderPanel<5>, &RenderPanel<6>, &RenderPanel<7>,
			&RenderPanel<8>, &RenderPanel<9>, &RenderPanel<10>, &RenderPanel<11>,
			&RenderPanel<12>, &RenderPanel<13>, &RenderPanel<14>, &RenderPanel<15>
		};

		// Every exported name the drawing above resolves. The header's wrappers call through a null
		// pointer when the framework lacks one, so registration is refused unless all are present
		// (the same guard Dragon's Eye Minimap uses; LOGIC-LIBRARY entry 49).
		bool HasRequiredExports(const char*& a_missing)
		{
			constexpr const char* required[] = {
				"AddSectionItem",
				"igTextV", "igTextDisabledV", "igTextWrappedV", "igSetTooltipV", "igSeparatorText", "igSeparator",
				"igSliderFloat", "igSliderInt", "igColorEdit4", "igCombo_Str_arr", "igInputText",
				"igButton", "igSmallButton", "igSameLine", "igPushID_Str", "igPushID_Ptr", "igPushID_Int", "igPopID",
				"igBeginDisabled", "igEndDisabled", "igIsItemHovered", "igIsItemActive", "igIsItemDeactivatedAfterEdit",
				"igSetNextItemWidth", "igBeginTabBar", "igEndTabBar", "igBeginTabItem", "igEndTabItem",
				// Toggle
				"igGetCursorScreenPos", "igGetWindowDrawList", "igGetFrameHeight", "igInvisibleButton",
				"ImDrawList_AddRectFilled", "ImDrawList_AddCircleFilled"
			};
			for (const char* name : required) {
				if (!GetMenuFrameworkFunction<void*>(name)) {
					a_missing = name;
					return false;
				}
			}
			return true;
		}
	}

	void Register()
	{
		if (g_hosted) {
			return;
		}
		HMODULE module = GetMenuFrameworkModule();
		if (!module) {
			logger::info("[AmfPage] no menu framework module is loaded; the settings stay on Wheeler's own page");
			return;
		}
		const char* missing = nullptr;
		if (!HasRequiredExports(missing)) {
			logger::warn("[AmfPage] framework module found but export '{}' is missing - not registering (Wheeler's own page stays)", missing ? missing : "?");
			return;
		}
		const auto& panels = PageModel::GetSingleton().Panels();
		if (panels.empty()) {
			logger::warn("[AmfPage] the page model has no panels; nothing to register");
			return;
		}
		g_panels.clear();
		SKSEMenuFramework::SetSection(kSectionName);
		int registered = 0;
		for (const auto& panelTab : panels) {
			if (!panelTab.panel || registered >= kMaxPanels) {
				continue;
			}
			g_panels.push_back(&panelTab);
			SKSEMenuFramework::AddSectionItem(panelTab.label, kTrampolines[registered]);
			++registered;
		}
		// Which module answered: the same preference order the consumer header resolves with.
		g_hostedBy = GetModuleHandleW(L"!ApocryphaMenuFramework") ? "!ApocryphaMenuFramework" :
		             (GetModuleHandleW(L"ApocryphaMenuFramework") ? "ApocryphaMenuFramework" : "SKSEMenuFramework");
		g_hosted = registered > 0;
		logger::info("[AmfPage] registered {} section(s) under '{}' with {}", registered, kSectionName, g_hostedBy);
		SyncAdvancedVisibility();   // the INI's Advanced settings state, before the menu is first drawn
	}

	bool IsHosted()
	{
		return g_hosted;
	}

	const char* HostedBy()
	{
		return g_hostedBy.c_str();
	}
}
