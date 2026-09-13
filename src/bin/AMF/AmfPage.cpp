#include "bin/AMF/AmfPage.h"
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

		constexpr const char* kSectionName = "Wheeler - Refined";
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
				Shared::BeginCapture(a_entry.iniKey);
			}
			MCP::EndDisabled();

			const std::string notice = Shared::RowNotice(a_entry.iniKey);
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
		}

		void DrawTab(const Panel& a_panel, const Tab& a_tab)
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
				if (MCP::BeginTabBar("sub", MCP::ImGuiTabBarFlags_FittingPolicyScroll | MCP::ImGuiTabBarFlags_TabListPopupButton)) {
					for (const auto& child : a_tab.children) {
						if (MCP::BeginTabItem(child.label.c_str())) {
							DrawTab(a_panel, child);
							MCP::EndTabItem();
						}
					}
					MCP::EndTabBar();
				}
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
					if (entry.type == EntryType::Keymap && entry.HasIniTarget() && Shared::IsCapturing(entry.iniKey)) {
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

		void RenderPanelIndex(int a_index)
		{
			if (a_index < 0 || a_index >= static_cast<int>(g_panels.size()) || !g_panels[a_index] || !g_panels[a_index]->panel) {
				MCP::TextDisabled("This panel is not available.");
				return;
			}
			const PanelTab& panelTab = *g_panels[a_index];
			MCP::PushID(a_index);
			ConsumeArmedRowAnywhere();
			if (panelTab.label.find("Wheel Behavior") != std::string::npos) {
				DrawLiveSlotCount();
			}
			if (MCP::BeginTabBar("tabs", MCP::ImGuiTabBarFlags_FittingPolicyScroll | MCP::ImGuiTabBarFlags_TabListPopupButton)) {
				for (const auto& tab : panelTab.tabs) {
					if (MCP::BeginTabItem(tab.label.c_str())) {
						DrawTab(*panelTab.panel, tab);
						MCP::EndTabItem();
					}
				}
				MCP::EndTabBar();
			}
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
