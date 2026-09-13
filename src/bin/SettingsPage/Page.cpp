#include "Page.h"

#include "Descriptor.h"
#include "PageInput.h"
#include "PageLogic.h"
#include "PageModel.h"
#include "PageShared.h"
#include "ValueStore.h"

#include "bin/AMF/AMFLaunch.h"
#include "bin/Config.h"
#include "bin/SettingsPresets.h"
#include "bin/UserInput/Controls.h"

// For the descriptor buttons' actions. Each was confirmed to be declared in the header named, not
// assumed - Page.cpp previously included neither, and a missing include here is a compile error
// rather than a silent one only because the calls are direct.
#include "bin/Texts.h"
#include "bin/Utilities/Utils.h"
#include "bin/Wheeler/AmmoWheelReskinUnified.h"
#include "bin/Wheeler/Wheeler.h"

// Included explicitly rather than leaned on: imgui.h does arrive transitively (through Config.h,
// and the PCH force-includes it in the DLL), but this file's whole job is ImGui calls, and a file
// should not depend on someone else's include for its primary dependency.
#include "imgui.h"

// Same reasoning as imgui.h above: the PCH does pull SimpleIni in, but LoadStartupFlag uses
// CSimpleIniA directly, so this file declares its own dependency rather than borrowing one.
#include <SimpleIni.h>

#include <algorithm>
#include <functional>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace SettingsPage
{
	namespace Page
	{
		namespace
		{
			// Atomic because the toggle key made this cross-thread: SetOpen is now reached from the
			// INPUT thread (Input::ProcessAndFilter -> Page::Toggle), while Draw and the present
			// hook's IsOpen() guard read it every frame on the RENDER thread.
			std::atomic<bool> g_open{ false };

			// ---- driving from devbench ---------------------------------------------------
			// devbench calls tool handlers on its own listener thread, and most of this page is
			// render-thread state: ValueStore does file I/O, g_capturingKey is a plain std::string.
			// So a request is queued here and RUN on the render thread at the top of Draw().
			//
			// The waiting side takes a deadline. Blocking forever would hang devbench's listener
			// thread any time rendering stalls - a load screen, a pause, a modal - and a hung tool
			// thread is much harder to diagnose than a reported timeout.
			struct DriveCommand
			{
				enum class Kind
				{
					SelectTab,
					Get,
					Set,
					Rebind
				};

				Kind kind{ Kind::Get };
				std::string key;    // ini key (or "Section/Key"), or the panel label for SelectTab
				std::string value;  // raw value to write, or the tab label for SelectTab
				std::uint32_t code{ 0 };
				bool cancel{ false };

				std::string result;
				bool done{ false };
				// Set when the caller gave up waiting. The drain skips these rather than doing file
				// I/O for a result nobody will read.
				bool abandoned{ false };
			};

			std::mutex g_driveMutex;
			std::condition_variable g_driveCv;
			std::vector<std::shared_ptr<DriveCommand>> g_driveQueue;

			// Honoured on the next frame through ImGuiTabItemFlags_SetSelected. ImGui owns tab
			// selection, so there is no other way to move the page to a given tab.
			std::string g_pendingPanelTab;
			std::string g_pendingTab;

			// Escapes the characters JSON forbids raw. Small on purpose: every string that reaches
			// it is a descriptor label or an INI value, never arbitrary binary.
			std::string JsonEscape(const std::string& a_text)
			{
				std::string out;
				out.reserve(a_text.size() + 8);
				for (const char ch : a_text) {
					switch (ch) {
					case '"':  out += "\\\""; break;
					case '\\': out += "\\\\"; break;
					case '\n': out += "\\n"; break;
					case '\r': out += "\\r"; break;
					case '\t': out += "\\t"; break;
					default:
						if (static_cast<unsigned char>(ch) < 0x20) {
							char buf[8]{};
							std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned>(ch) & 0xFFu);
							out += buf;
						} else {
							out += ch;
						}
						break;
					}
				}
				return out;
			}

			// Depth-first search for a control by its INI key. Accepts "Section/Key" as well as a
			// bare key, because a key alone is NOT unique across eight descriptors - Wheeler's
			// Controls.ini and AmmoWheel.ini both carry a `toggleWheel`, for instance.
			bool FindControlInEntries(const std::vector<Entry>& a_entries, const std::string& a_section,
				const std::string& a_key, const Entry*& a_out)
			{
				for (const auto& entry : a_entries) {
					if (entry.HasIniTarget() && entry.iniKey == a_key &&
					    (a_section.empty() || entry.iniSection == a_section)) {
						a_out = &entry;
						return true;
					}
					if (!entry.entries.empty() &&
					    FindControlInEntries(entry.entries, a_section, a_key, a_out)) {
						return true;
					}
				}
				return false;
			}

			bool FindControl(const std::string& a_spec, const Panel*& a_panel, const Entry*& a_entry)
			{
				std::string section;
				std::string key = a_spec;
				if (const auto slash = a_spec.find('/'); slash != std::string::npos) {
					section = a_spec.substr(0, slash);
					key = a_spec.substr(slash + 1);
				}

				for (const auto& panel : Catalog::GetSingleton().Panels()) {
					const Entry* found = nullptr;
					if (FindControlInEntries(panel.entries, section, key, found)) {
						a_panel = &panel;
						a_entry = found;
						return true;
					}
				}
				return false;
			}

			// Applying a change. Upstream runs ONE fixed sequence for every panel - see
			// ModCallbackEventHandler::ProcessEvent - so the page reuses it rather than inventing a
			// per-descriptor map. Only the public Config entry points are called; the three
			// descriptors with no public reader of their own (wheelBehavior via the file-static
			// ReadWheelBehaviorConfig, I4 inside ReadStyleConfig, the bridge layout inside
			// ReadActionHotkeysBridgeConfig) are all covered by this sequence.
			//
			// TODO, recorded in the plan: ProcessEvent's body should be factored into a shared
			// function that both it and this call, so the two cannot drift. Until then this is a
			// deliberate duplicate of its LIGHTWEIGHT path. The heavy close/reopen cycle is not
			// reproduced here.
			//
			// For KEYBINDS that is now known to be safe rather than assumed: a rebind driven in game
			// on 2026-09-12 (SE 1.5.97) took effect immediately - the new code opened the wheel and
			// the old code stopped working - so BindAllInputsFromConfig plus this sequence is enough.
			// The heavy cycle's remaining justification is the slow-time scale, which is untested.
			void ApplyChangedSettings()
			{
				Config::ReadStyleConfig();
				Config::ReadControlConfig();
				Config::ReadActionHotkeysBridgeConfig();
				Config::ReadOStimIntegrationConfig();
				Config::ReadAmmoWheelConfig();

				Config::ResetScaleBaseCapture();
				Config::OffsetSizingToViewport();
				Config::OffsetAmmoWheelSizingToViewport();

				Controls::BindAllInputsFromConfig();
			}

			// The INI stores raw scan codes (Controls.defaults.ini: toggleWheel = 278), so a keymap
			// row has to be turned into a name. Which naming table applies is decided by the
			// section, since gamepad and keyboard codes overlap - a heuristic, but the descriptor
			// offers nothing better and the sections are explicit: InputBindings.GamePad,
			// InputBindings.MKB, ActionHotkeysBridge.*, Input.
			bool LooksLikeGamepad(const Entry& a_entry)
			{
				std::string haystack = a_entry.iniSection + "/" + a_entry.iniKey;
				std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) {
					return static_cast<char>(std::tolower(c));
				});
				return haystack.find("gamepad") != std::string::npos;
			}

			void HelpMarker(const Entry& a_entry)
			{
				if (!a_entry.desc.empty() && ImGui::IsItemHovered()) {
					ImGui::SetItemTooltip("%s", a_entry.desc.c_str());
				}
			}

			void DrawEntry(const Panel& a_panel, const Entry& a_entry);

			// A descriptor button carries its action id at the TOP level of the entry - the one
			// place ParseControl did not read - so until that was fixed every button's id was
			// empty. The failure mode that nearly shipped is the one guarded against here: a
			// control that looks live and does nothing. An id with no action stays DISABLED.
			//
			// The bodies deliberately mirror ModCallbackEventHandler's dmenu_buttonCallback cases
			// rather than paraphrasing them; a subtly different reset is worse than no reset.
			void RunButtonAction(const std::string& id);
			bool IsKnownButtonId(const std::string& id);

			void DrawButton(const Entry& a_entry)
			{
				const std::string& id = a_entry.id;

				const bool known =
					id == "wheeler_reset_all_wheels" ||
					id == "wheeler_actionhotkeysbridge_reset_layout" ||
					id == "wheeler_ammowheel_rebind_reset" ||
					id == "wheeler_ammowheel_rebind_cancel" ||
					id == "wheeler_ammowheel_restore_factory_defaults";

				if (!known) {
					ImGui::BeginDisabled(true);
					ImGui::Button(a_entry.name.c_str());
					ImGui::EndDisabled();
					if (ImGui::IsItemHovered()) {
						ImGui::SetItemTooltip("No action is wired to this button%s.",
							id.empty() ? "" : (" (id: " + id + ")").c_str());
					}
					return;
				}

				if (!ImGui::Button(a_entry.name.c_str())) {
					return;
				}
				RunButtonAction(id);
			}

			bool IsKnownButtonId(const std::string& id)
			{
				return id == "wheeler_reset_all_wheels" ||
				       id == "wheeler_actionhotkeysbridge_reset_layout" ||
				       id == "wheeler_ammowheel_rebind_reset" ||
				       id == "wheeler_ammowheel_rebind_cancel" ||
				       id == "wheeler_ammowheel_restore_factory_defaults";
			}

			// The action behind a descriptor button, shared with the framework-hosted page (M9).
			void RunButtonAction(const std::string& id)
			{
				if (id == "wheeler_reset_all_wheels") {
					Wheeler::SetupDefaultWheels();
				} else if (id == "wheeler_actionhotkeysbridge_reset_layout") {
					Wheeler::ResetActionHotkeysBridgeLayout();
				} else if (id == "wheeler_ammowheel_rebind_cancel") {
					// No upstream case exists for this id, but Controls::CancelRebind() is
					// unambiguously what it names, and the alternative is a dead button.
					Controls::CancelRebind();
					Utils::NotificationMessage(Texts::GetText(Texts::TextType::KeybindCaptureCancelled));
				} else if (id == "wheeler_ammowheel_rebind_reset") {
					Config::AmmoWheel::MKB::toggleAmmoWheel = 42;
					Config::AmmoWheel::GamePad::toggleAmmoWheel = 269;   // D-pad right, the shipped default
					Config::AmmoWheel::MKB::modifierKey = 0;
					Config::AmmoWheel::GamePad::modifierButton = 0;
					Config::AmmoWheel::MKB::toggleAmmoWheelMouse = 0;
					Config::AmmoWheel::ToggleKeyMKBName =
						Controls::GetKeyNameForMkb(Config::AmmoWheel::MKB::toggleAmmoWheel);
					Config::AmmoWheel::ToggleKeyGamepadName =
						Controls::GetKeyNameForGamepad(Config::AmmoWheel::GamePad::toggleAmmoWheel);
					Config::AmmoWheel::ModifierKeyMKBName = "None";
					Config::AmmoWheel::ModifierButtonGamepadName = "None";
					Config::AmmoWheel::ToggleMouseButtonName = "None";
					Config::WriteAmmoWheelKeybindOverrides();
					Controls::BindAllInputsFromConfig();
					Utils::NotificationMessage(Texts::GetText(Texts::TextType::AmmoWheelKeybindsReset));
				} else if (id == "wheeler_ammowheel_restore_factory_defaults") {
					Controls::CancelRebind();
					if (Config::AmmoWheel::RestoreFactoryDefaults()) {
						Config::ReadAmmoWheelConfig();
						AmmoWheelReskinUnified::ReskinSystem::GetSingleton().ReloadSmartFromIni();
						Config::OffsetAmmoWheelSizingToViewport();

						Config::AmmoWheel::ToggleKeyMKBName =
							Controls::GetKeyNameForMkb(Config::AmmoWheel::MKB::toggleAmmoWheel);
						Config::AmmoWheel::ToggleKeyGamepadName =
							Controls::GetKeyNameForGamepad(Config::AmmoWheel::GamePad::toggleAmmoWheel);
						Config::AmmoWheel::ModifierKeyMKBName = Config::AmmoWheel::MKB::modifierKey ?
							Controls::GetKeyNameForMkb(Config::AmmoWheel::MKB::modifierKey) : "None";
						Config::AmmoWheel::ModifierButtonGamepadName = Config::AmmoWheel::GamePad::modifierButton ?
							Controls::GetKeyNameForGamepad(Config::AmmoWheel::GamePad::modifierButton) : "None";
						Config::AmmoWheel::ToggleMouseButtonName = Config::AmmoWheel::MKB::toggleAmmoWheelMouse ?
							Controls::GetKeyNameForMkb(Config::AmmoWheel::MKB::toggleAmmoWheelMouse) : "None";

						Controls::BindAllInputsFromConfig();
						Wheeler::NotifyAmmoWheelConfigChanged();
						Utils::NotificationMessage(Texts::GetText(Texts::TextType::AmmoWheelFactoryDefaultsRestored));
					} else {
						Utils::NotificationMessage(Texts::GetText(Texts::TextType::AmmoWheelFactoryDefaultsFailed));
					}
				}

				// The value store's view of the INI is now stale for anything these wrote.
				ValueStore::GetSingleton().LoadAll(Catalog::GetSingleton());
			}

			// Rule 32 / PREFLIGHT: a boolean setting is drawn as an on/off SWITCH, never a checkbox.
			// ImGui ships no toggle widget, so this is the house one. Sized from the current frame
			// height so it scales with the page's font rather than being a fixed pixel size.
			bool ToggleSwitch(const char* a_label, bool* a_value)
			{
				ImGui::PushID(a_label);

				const float height = ImGui::GetFrameHeight();
				const float width = height * 1.85f;
				const float radius = height * 0.5f;

				const ImVec2 pos = ImGui::GetCursorScreenPos();
				ImDrawList* draw = ImGui::GetWindowDrawList();

				bool changed = false;
				if (ImGui::InvisibleButton("##switch", ImVec2(width, height))) {
					*a_value = !*a_value;
					changed = true;
				}

				const bool hovered = ImGui::IsItemHovered();
				const ImU32 track = *a_value ?
					ImGui::GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_ButtonActive) :
					ImGui::GetColorU32(hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);

				draw->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), track, radius);
				const float knobX = *a_value ? (pos.x + width - radius) : (pos.x + radius);
				draw->AddCircleFilled(ImVec2(knobX, pos.y + radius), radius - 2.0f,
					ImGui::GetColorU32(*a_value ? ImGuiCol_CheckMark : ImGuiCol_TextDisabled));

				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(a_label);

				ImGui::PopID();
				return changed;
			}

			void DrawCheckbox(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
			{
				bool value = a_current.AsBool(a_entry.defaultBool.value_or(false));
				if (ToggleSwitch(a_entry.name.c_str(), &value)) {
					if (ValueStore::GetSingleton().Set(a_panel, a_entry,
							FormatBoolLike(value, a_current))) {
						ApplyChangedSettings();
					}
				}
			}

			void DrawSlider(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
			{
				const double fallback = a_entry.defaultNumber.value_or(0.0);
				double value = a_current.AsNumber(fallback);

				double minimum = 0.0;
				double maximum = 1.0;
				double step = 0.0;
				if (a_entry.slider) {
					minimum = a_entry.slider->min;
					maximum = a_entry.slider->max;
					step = a_entry.slider->step;
				}

				// A whole-number step means a whole-number control. 486 sliders, and a good share of
				// them are counts and indices where a float slider would let the player pick 3.4.
				const bool integral = (step >= 1.0) && (step == static_cast<double>(static_cast<long long>(step)));

				bool changed = false;
				if (integral) {
					int shown = static_cast<int>(value);
					changed = ImGui::SliderInt(a_entry.name.c_str(), &shown,
						static_cast<int>(minimum), static_cast<int>(maximum));
					if (changed) {
						value = static_cast<double>(shown);
					}
				} else {
					float shown = static_cast<float>(value);
					changed = ImGui::SliderFloat(a_entry.name.c_str(), &shown,
						static_cast<float>(minimum), static_cast<float>(maximum), "%.3f");
					if (changed) {
						value = static_cast<double>(shown);
					}
				}

				if (changed) {
					if (ValueStore::GetSingleton().Set(a_panel, a_entry,
							FormatNumberLike(value, a_current))) {
						ApplyChangedSettings();
					}
				}
			}

			void DrawColor(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
			{
				ColorValue colour = a_current.AsColor(a_entry.defaultColor.value_or(ColorValue{}));
				float rgba[4] = { colour.r, colour.g, colour.b, colour.a };

				// The preset trap: a colour whose sibling <key>Preset is non-zero is overwritten
				// from a palette every load, so editing it changes nothing on screen. The descriptor
				// does not encode this, so the page says so plainly rather than letting a player
				// pick a colour and watch nothing happen.
				const bool overridden = IsOverriddenByPreset(a_panel, a_entry, ValueStore::GetSingleton());
				if (overridden) {
					ImGui::BeginDisabled(true);
				}

				if (ImGui::ColorEdit4(a_entry.name.c_str(), rgba)) {
					colour.r = rgba[0];
					colour.g = rgba[1];
					colour.b = rgba[2];
					colour.a = rgba[3];
					if (ValueStore::GetSingleton().Set(a_panel, a_entry,
							FormatColorLike(colour, a_current))) {
						ApplyChangedSettings();
					}
				}

				if (overridden) {
					ImGui::EndDisabled();
					ImGui::SameLine();
					ImGui::TextDisabled("(a preset is overriding this)");
					if (ImGui::IsItemHovered()) {
						ImGui::SetItemTooltip(
							"%s is set to a palette preset, which replaces this colour on every load.\n"
							"Set that preset to Custom to use the colour chosen here.",
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

				if (ImGui::Combo(a_entry.name.c_str(), &index, items.data(), static_cast<int>(items.size()))) {
					if (ValueStore::GetSingleton().Set(a_panel, a_entry,
							FormatIndexLike(index, a_current))) {
						ApplyChangedSettings();
					}
				}
			}

			void DrawTextbox(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
			{
				std::array<char, 512> buffer{};
				const std::string current = a_current.AsString(a_entry.defaultString.value_or(std::string{}));
				std::strncpy(buffer.data(), current.c_str(), buffer.size() - 1);

				if (ImGui::InputText(a_entry.name.c_str(), buffer.data(), buffer.size(),
						ImGuiInputTextFlags_EnterReturnsTrue)) {
					if (ValueStore::GetSingleton().Set(a_panel, a_entry, std::string(buffer.data()))) {
						ApplyChangedSettings();
					}
				}
			}

			// Which keymap row armed capture. The capture state itself lives in PageInput, because it
			// is read on the INPUT thread; this is only the render-thread record of WHICH row is
			// waiting, so a captured code is written to the row the player actually clicked.
			//
			// Keyed by INI key rather than by Entry address: the ini key is unique across the
			// catalogue and survives a reload, whereas the model (and every Entry in it) is rebuilt.
			std::string g_capturingKey;

			// A notice that stays on its row until that row is armed again or binds: a one-frame
			// TextDisabled after a refused capture is invisible at 120 FPS.
			std::string g_rowNoticeKey;
			std::string g_rowNotice;

			enum class CaptureOutcome
			{
				NotPending,   // this row never armed
				Waiting,      // armed, nothing captured yet
				Cancelled,    // the player pressed Escape or Home
				Bound,        // a code was captured and written
				Reserved,     // captured, but the menu framework reserves that key (the owner, 2026-09-12)
				InUse,        // captured, but another Wheeler action on the same device already holds it (the owner, same day)
				WriteFailed   // captured, but ValueStore::Set refused (see its read-back check)
			};

			// The owner, 2026-09-12: "within the mod ... two separate functions cannot be bound to the same
			// key". Every keymap row in every descriptor is checked, on the SAME device as the row being
			// bound (a gamepad code and a keyboard code can never collide - the stored ranges differ).
			// Modifier rows count too: a chord key that is also a plain action fires both. Returns the
			// name of the row that holds the code, or empty when nothing does.
			std::string FindOtherActionOnCode(const Entry& a_self, bool a_gamepad, std::uint32_t a_code)
			{
				std::string holder;
				const auto& store = ValueStore::GetSingleton();
				std::function<void(const Panel&, const Entry&)> walk = [&](const Panel& panel, const Entry& entry) {
					if (!holder.empty()) {
						return;
					}
					for (const Entry& child : entry.entries) {
						walk(panel, child);
					}
					if (entry.type != EntryType::Keymap || !entry.HasIniTarget()) {
						return;
					}
					if (entry.iniSection == a_self.iniSection && entry.iniKey == a_self.iniKey) {
						return;   // the row being bound
					}
					if (LooksLikeGamepad(entry) != a_gamepad) {
						return;
					}
					const auto held = static_cast<std::uint32_t>(
						store.Get(panel, entry).AsNumber(entry.defaultNumber.value_or(0.0)));
					if (held != 0u && held == a_code) {
						holder = entry.name.empty() ? (entry.iniSection + "/" + entry.iniKey) : entry.name;
					}
				};
				for (const Panel& panel : Catalog::GetSingleton().Panels()) {
					for (const Entry& entry : panel.entries) {
						walk(panel, entry);
					}
				}
				return holder;
			}

			// Takes whatever capture produced for this row and, if it is a real code, writes it.
			//
			// Shared by the widget and the devbench driver ON PURPOSE: a driven rebind must run the
			// same code a clicked one does, or the test proves a path the player never takes.
			//
			// The pending test deliberately does NOT consult PageInput::IsCapturingKeymap(). A
			// SUCCESSFUL capture clears the armed flag, so gating on it meant the frame that had a
			// code to collect was exactly the frame that stopped looking - the code was never taken,
			// never written, and sat in the atomic ready to fire against the NEXT row armed.
			CaptureOutcome ConsumeCapture(const Panel& a_panel, const Entry& a_entry,
				const ResolvedValue& a_current, std::uint32_t& a_boundCode)
			{
				if (a_entry.iniKey.empty() || g_capturingKey != a_entry.iniKey) {
					return CaptureOutcome::NotPending;
				}

				// Both are drained every time. A cancel left over from an earlier attempt would
				// otherwise fire against this one.
				const std::uint32_t captured = PageInput::TakeCapturedKey();
				const bool cancelled = PageInput::TakeCaptureCancelled();

				if (cancelled) {
					g_capturingKey.clear();
					return CaptureOutcome::Cancelled;
				}
				if (captured == 0u) {
					return CaptureOutcome::Waiting;
				}

				g_capturingKey.clear();

				// The framework's keys are off limits (the owner, 2026-09-12: our mods never take AMF's
				// menu key or the others it reserves). Asked fresh at every capture so the answer is
				// whatever AMF has bound NOW, not what it shipped with. Refused before the write, so
				// neither the INI nor the dispatcher ever holds the reserved code.
				AMFLaunch::RefreshReservedKeys();
				if (AMFLaunch::IsKeyReservedByFramework(captured)) {
					logger::info("[SettingsPage] rebind of {} refused: code {} is reserved by the menu framework", a_entry.iniKey, captured);
					g_rowNoticeKey = a_entry.iniKey;
					g_rowNotice = Texts::GetText(Texts::TextType::KeymapReservedByFramework);
					return CaptureOutcome::Reserved;
				}

				// Same mod, same device, same code, different action: refused, naming the holder.
				const std::string holder = FindOtherActionOnCode(a_entry, LooksLikeGamepad(a_entry), captured);
				if (!holder.empty()) {
					logger::info("[SettingsPage] rebind of {} refused: code {} is already bound to '{}'", a_entry.iniKey, captured, holder);
					g_rowNoticeKey = a_entry.iniKey;
					g_rowNotice = std::string(Texts::GetText(Texts::TextType::KeymapAlreadyBoundTo)) + " " + holder;
					return CaptureOutcome::InUse;
				}

				a_boundCode = captured;

				if (!ValueStore::GetSingleton().Set(a_panel, a_entry,
						FormatIndexLike(static_cast<int>(captured), a_current))) {
					g_rowNoticeKey = a_entry.iniKey;
					g_rowNotice = "(write failed)";
					return CaptureOutcome::WriteFailed;
				}
				if (g_rowNoticeKey == a_entry.iniKey) {
					g_rowNoticeKey.clear();
					g_rowNotice.clear();
				}

				// Writing the INI alone changes nothing in a running game: the dispatcher holds a
				// table built from the bound values, and this is what rebuilds it.
				//
				// PROVEN LIVE (2026-09-12, Apostasy SE 1.5.97, driven through devbench): rebinding
				// InputBindings.MKB/toggleWheel from 34 to 36 and then pressing 36 opened the wheel,
				// and pressing the OLD code 34 afterwards did nothing. Both halves matter - the new
				// binding works AND the old one is genuinely gone - so BindAllInputsFromConfig plus
				// the lightweight ApplyChangedSettings is sufficient for a keymap change. The heavy
				// close/reopen cycle the note above mentions is NOT needed for this case.
				Controls::BindAllInputsFromConfig();
				ApplyChangedSettings();
				return CaptureOutcome::Bound;
			}

			void DrawKeymap(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current)
			{
				std::uint32_t bound = 0;
				const CaptureOutcome outcome = ConsumeCapture(a_panel, a_entry, a_current, bound);

				// Read the value AFTER consuming, so a rebind shows its new key on the same frame
				// rather than one frame stale.
				const auto current = (outcome == CaptureOutcome::Bound) ?
					bound :
					static_cast<std::uint32_t>(a_current.AsNumber(a_entry.defaultNumber.value_or(0.0)));

				const std::string name = LooksLikeGamepad(a_entry) ?
					Controls::GetKeyNameForGamepad(current) :
					Controls::GetKeyNameForMkb(current);

				ImGui::Text("%s", a_entry.name.c_str());
				ImGui::SameLine();

				if (outcome == CaptureOutcome::Waiting) {
					ImGui::TextDisabled("press a key...");
					ImGui::SameLine();
					if (ImGui::SmallButton("Cancel")) {
						PageInput::CancelKeymapCapture();
						g_capturingKey.clear();
					}
					return;
				}

				ImGui::TextDisabled("%s", name.c_str());
				ImGui::SameLine();
				// Disabled while ANOTHER row is pending: only one keymap may be armed at a time, and
				// arming a second would silently steal the first one's capture.
				ImGui::BeginDisabled(!g_capturingKey.empty());
				if (ImGui::SmallButton("Rebind")) {
					PageInput::BeginKeymapCapture();
					g_capturingKey = a_entry.iniKey;
					if (g_rowNoticeKey == a_entry.iniKey) {
						g_rowNoticeKey.clear();
						g_rowNotice.clear();
					}
				}
				ImGui::EndDisabled();

				if (g_rowNoticeKey == a_entry.iniKey && !g_rowNotice.empty()) {
					ImGui::SameLine();
					ImGui::TextDisabled("%s", g_rowNotice.c_str());
				} else if (!LooksLikeGamepad(a_entry) && AMFLaunch::IsKeyReservedByFramework(current)) {
					// The stored value collides with the framework (an INI edit or an old default): the
					// dispatcher left it unbound at load, and the row says so instead of showing a key
					// name that does nothing.
					ImGui::SameLine();
					ImGui::TextDisabled("%s", Texts::GetText(Texts::TextType::KeymapReservedByFramework));
				}
			}

			void DrawEntry(const Panel& a_panel, const Entry& a_entry)
			{
				// A group reaching here was small enough to be flattened inline, so it keeps its
				// heading and its children follow in order.
				if (a_entry.type == EntryType::Group) {
					if (!a_entry.name.empty()) {
						ImGui::SeparatorText(a_entry.name.c_str());
					}
					return;
				}

				if (a_entry.type == EntryType::Text) {
					if (!a_entry.name.empty()) {
						ImGui::TextWrapped("%s", a_entry.name.c_str());
					}
					if (!a_entry.desc.empty()) {
						ImGui::TextDisabled("%s", a_entry.desc.c_str());
					}
					return;
				}

				const auto visibility =
					EvaluateVisibility(Catalog::GetSingleton(), a_panel, a_entry, ValueStore::GetSingleton());
				if (visibility == Visibility::Hidden) {
					return;
				}

				const bool disabled = (visibility == Visibility::Disabled);
				if (disabled) {
					ImGui::BeginDisabled(true);
				}

				ImGui::PushID(a_entry.iniKey.empty() ? a_entry.name.c_str() : a_entry.iniKey.c_str());

				const auto current = ValueStore::GetSingleton().Get(a_panel, a_entry);

				switch (a_entry.type) {
				case EntryType::Checkbox:
					DrawCheckbox(a_panel, a_entry, current);
					break;
				case EntryType::Slider:
					DrawSlider(a_panel, a_entry, current);
					break;
				case EntryType::Color:
					DrawColor(a_panel, a_entry, current);
					break;
				case EntryType::Dropdown:
					DrawDropdown(a_panel, a_entry, current);
					break;
				case EntryType::Textbox:
					DrawTextbox(a_panel, a_entry, current);
					break;
				case EntryType::Keymap:
					DrawKeymap(a_panel, a_entry, current);
					break;
				case EntryType::Button:
					DrawButton(a_entry);
					break;
				default:
					break;
				}

				HelpMarker(a_entry);
				ImGui::PopID();

				if (disabled) {
					ImGui::EndDisabled();
				}
			}

			// Settings presets (the owner, 2026-09-13): the shipped defaults, named user presets, and
			// the preset THIS SAVE uses. Drawn at the top of Wheeler Controls / General on both
			// surfaces; the state (name box, notice, selection) lives in SettingsPresets so the two
			// pages cannot disagree about what is selected or what just happened.
			void DrawPresetsPanel()
			{
				ImGui::PushID("settings-presets");
				ImGui::SeparatorText(Texts::GetText(Texts::TextType::PresetsHeader));
				ImGui::TextWrapped("%s", Texts::GetText(Texts::TextType::PresetsHelp));
				const auto names = SettingsPresets::List();
				const std::string current = SettingsPresets::SavePreset();
				std::vector<const char*> items;
				items.push_back(Texts::GetText(Texts::TextType::PresetNone));
				int index = 0;
				for (std::size_t i = 0; i < names.size(); ++i) {
					items.push_back(names[i].c_str());
					if (names[i] == current) {
						index = static_cast<int>(i) + 1;
					}
				}
				ImGui::SetNextItemWidth(280.0f);
				if (ImGui::Combo(Texts::GetText(Texts::TextType::PresetForSave), &index, items.data(), static_cast<int>(items.size()))) {
					std::string err;
					if (index <= 0) {
						SettingsPresets::ClearSavePreset();
						SettingsPresets::SetNotice(Texts::GetText(Texts::TextType::PresetClearedForSave));
					} else if (SettingsPresets::Load(names[index - 1], err)) {
						SettingsPresets::SetSavePreset(names[index - 1]);
						SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetLoaded)) + " " + names[index - 1]);
					} else {
						SettingsPresets::SetNotice(err);
					}
				}
				ImGui::SetNextItemWidth(280.0f);
				ImGui::InputText(Texts::GetText(Texts::TextType::PresetNameLabel), SettingsPresets::NameBuffer(), SettingsPresets::NameBufferSize());
				ImGui::SameLine();
				if (ImGui::Button(Texts::GetText(Texts::TextType::PresetSaveAs))) {
					std::string name = SettingsPresets::NameBuffer();
					if (name.find_first_not_of(' ') == std::string::npos) {
						name = current;   // nothing typed: overwrite the save's own preset
					}
					std::string err;
					if (SettingsPresets::SaveAs(name, err)) {
						SettingsPresets::SetSavePreset(name);
						SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetSaved)) + " " + SettingsPresets::SavePreset());
					} else {
						SettingsPresets::SetNotice(err);
					}
				}
				const bool none = current.empty();
				if (none) {
					ImGui::BeginDisabled(true);
				}
				if (ImGui::Button(Texts::GetText(Texts::TextType::PresetRename))) {
					const std::string to = SettingsPresets::NameBuffer();
					std::string err;
					if (SettingsPresets::Rename(current, to, err)) {
						SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetRenamed)) + " " + SettingsPresets::SavePreset());
					} else {
						SettingsPresets::SetNotice(err);
					}
				}
				ImGui::SameLine();
				if (ImGui::Button(Texts::GetText(Texts::TextType::PresetDelete))) {
					std::string err;
					if (SettingsPresets::Delete(current, err)) {
						SettingsPresets::SetNotice(std::string(Texts::GetText(Texts::TextType::PresetDeleted)) + " " + current);
					} else {
						SettingsPresets::SetNotice(err);
					}
				}
				if (none) {
					ImGui::EndDisabled();
				}
				ImGui::SameLine();
				if (ImGui::Button(Texts::GetText(Texts::TextType::PresetResetDefaults))) {
					std::string err;
					SettingsPresets::SetNotice(SettingsPresets::ResetToDefaults(err) ? Texts::GetText(Texts::TextType::PresetDefaultsRestored) : err.c_str());
				}
				ImGui::SameLine();
				if (ImGui::Button(Texts::GetText(Texts::TextType::PresetReloadIni))) {
					SettingsPresets::ReloadFromIni();
					SettingsPresets::SetNotice(Texts::GetText(Texts::TextType::PresetReloaded));
				}
				const std::string notice = SettingsPresets::Notice();
				if (!notice.empty()) {
					ImGui::TextWrapped("%s", notice.c_str());
				}
				ImGui::Separator();
				ImGui::PopID();
			}

			void DrawLiveSlotCount()
			{
				const int live = Wheeler::GetCurrentWheelSlotCount();
				if (live < 0) {   // -1 = no wheel; 0 = a wheel with no slots yet, which the slider can grow
					ImGui::TextDisabled("%s", Texts::GetText(Texts::TextType::SlotCountNoWheel));
					ImGui::SameLine();
					// PushWheel is the serialization-side constructor: no edit-mode guard, so the first wheel
					// can be made from the page without the Favorites-menu edit-mode dance.
					if (ImGui::Button(Texts::GetText(Texts::TextType::SlotCountCreateFirst))) {
						Wheeler::PushWheel();
						INFO("[SettingsPage] first wheel created from the page");
					}
					ImGui::Separator();
					return;
				}
				static int s_pending = 0;
				static bool s_dragging = false;
				if (!s_dragging) {
					s_pending = live;
				}
				ImGui::SetNextItemWidth(220.0f);
				ImGui::SliderInt(Texts::GetText(Texts::TextType::SlotCountLabel), &s_pending, 1, 64);
				s_dragging = ImGui::IsItemActive();
				if (ImGui::IsItemDeactivatedAfterEdit() && s_pending != live) {
					const int result = Wheeler::SetCurrentWheelSlotCount(s_pending);
					INFO("[SettingsPage] slot count {} -> requested {} -> now {}", live, s_pending, result);
					s_pending = result;
				}
				ImGui::TextDisabled("%s", Texts::GetText(Texts::TextType::SlotCountHelp));
				ImGui::Separator();
				static int s_pendingWheels = 0;
				static bool s_draggingWheels = false;
				const int liveWheels = Wheeler::GetWheelCount();
				if (!s_draggingWheels) {
					s_pendingWheels = liveWheels;
				}
				ImGui::SetNextItemWidth(220.0f);
				ImGui::SliderInt(Texts::GetText(Texts::TextType::WheelCountLabel), &s_pendingWheels, 1, 100);
				s_draggingWheels = ImGui::IsItemActive();
				if (ImGui::IsItemDeactivatedAfterEdit() && s_pendingWheels != liveWheels) {
					const int result = Wheeler::SetWheelCount(s_pendingWheels);
					INFO("[SettingsPage] wheel count {} -> requested {} -> now {}", liveWheels, s_pendingWheels, result);
					s_pendingWheels = result;
				}
				ImGui::TextDisabled("%s", Texts::GetText(Texts::TextType::WheelCountHelp));
				ImGui::Separator();
			}

			void DrawTab(const Panel& a_panel, const Tab& a_tab)
			{
				// ImGui identifies widgets by a hash of their label within the current ID scope, and
				// this function recurses - the same "sub" tab bar and "body" child ids appear at
				// every level. Only one path is drawn per frame so nothing collides today, but
				// scoping by the tab's own address costs nothing and removes the whole class of
				// state bleeding between tabs that share a control name.
				ImGui::PushID(static_cast<const void*>(&a_tab));

				struct ScopeGuard
				{
					~ScopeGuard() { ImGui::PopID(); }
				} popIdOnExit;

				if (!a_tab.desc.empty()) {
					ImGui::TextDisabled("%s", a_tab.desc.c_str());
					ImGui::Separator();
				}

				for (const auto* entry : a_tab.controls) {
					if (entry) {
						DrawEntry(a_panel, *entry);
					}
				}

				if (a_tab.children.empty()) {
					return;
				}

				// A second level, for the one panel that needs it: Ammo Wheel's 475 nodes.
				if (ImGui::BeginTabBar("sub",
						ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
					for (const auto& child : a_tab.children) {
						if (ImGui::BeginTabItem(child.label.c_str())) {
							ImGui::BeginChild("subbody", ImVec2(0, 0), false);
							DrawTab(a_panel, child);
							ImGui::EndChild();
							ImGui::EndTabItem();
						}
					}
					ImGui::EndTabBar();
				}
			}

			// ---- running a queued devbench request (render thread) -----------------------

			// Structure only - deliberately NO values. This is answered on devbench's listener
			// thread, and while the Catalog and PageModel are immutable after load, ValueStore is
			// not: Set mutates it. Reading values here would be the same cross-thread race this
			// page has already had to fix four times. `get` is queued onto the render thread and is
			// where values come from.
			// Appends one tab's controls, then RECURSES into its child tabs.
			//
			// The recursion is not decoration: a tab that delegates everything to sub-tabs has zero
			// direct controls, so without it "Input Binding" (0 direct, 2 children) and "Reskin
			// Layout" (0 direct, 6 children) listed as EMPTY - the tool reporting that a tab holding
			// dozens of settings had none. Each control carries the tab path it was found on.
			void AppendTabControls(const std::string& a_path, const Tab& a_tab, bool& a_first,
				std::string& a_out)
			{
				for (const auto* entry : a_tab.controls) {
					if (!entry || !IsControl(entry->type) || !entry->HasIniTarget()) {
						continue;
					}
					if (!a_first) {
						a_out += ",";
					}
					a_first = false;
					a_out += std::string(R"({"key":")") + JsonEscape(entry->iniKey) +
						R"(","section":")" + JsonEscape(entry->iniSection) +
						R"(","name":")" + JsonEscape(entry->name) +
						R"(","type":")" + EntryTypeName(entry->type) +
						R"(","tab":")" + JsonEscape(a_path) + R"("})";
				}

				for (const auto& child : a_tab.children) {
					AppendTabControls(a_path + " / " + child.label, child, a_first, a_out);
				}
			}

			std::string ListTabJson(const std::string& a_panelLabel, const Tab& a_tab)
			{
				std::string out = std::string(R"({"ok":true,"op":"list","panel":")") +
					JsonEscape(a_panelLabel) + R"(","tab":")" + JsonEscape(a_tab.label) +
					R"(","controls":[)";

				bool first = true;
				AppendTabControls(a_tab.label, a_tab, first, out);

				out += "]}";
				return out;
			}

			void RunDriveCommand(DriveCommand& a_command)
			{
				if (a_command.kind == DriveCommand::Kind::SelectTab) {
					g_pendingPanelTab = a_command.key;
					g_pendingTab = a_command.value;
					a_command.result = R"({"ok":true,"op":"selecttab","panel":")" +
						JsonEscape(a_command.key) + R"(","tab":")" + JsonEscape(a_command.value) +
						R"("})";
					return;
				}

				const Panel* panel = nullptr;
				const Entry* entry = nullptr;
				if (!FindControl(a_command.key, panel, entry) || !panel || !entry) {
					a_command.result =
						R"({"ok":false,"error":"no control with that ini key - try 'Section/Key'","key":")" +
						JsonEscape(a_command.key) + R"("})";
					return;
				}

				auto& store = ValueStore::GetSingleton();
				const auto current = store.Get(*panel, *entry);

				switch (a_command.kind) {
				case DriveCommand::Kind::Get:
					a_command.result =
						std::string(R"({"ok":true,"op":"get","panel":")") + JsonEscape(panel->name) +
						R"(","section":")" + JsonEscape(entry->iniSection) +
						R"(","key":")" + JsonEscape(entry->iniKey) +
						R"(","name":")" + JsonEscape(entry->name) +
						R"(","type":")" + EntryTypeName(entry->type) +
						R"(","raw":")" + JsonEscape(current.raw) +
						R"(","source":")" + ValueSourceName(current.source) + R"("})";
					break;

				case DriveCommand::Kind::Set:
					{
						// The same ValueStore::Set the widgets call, so its read-back check - the one
						// that catches an MO2-masked file swallowing the write - still applies.
						const bool ok = store.Set(*panel, *entry, a_command.value);
						if (ok) {
							ApplyChangedSettings();
						}
						const auto after = store.Get(*panel, *entry);
						a_command.result =
							std::string(R"({"ok":)") + (ok ? "true" : "false") +
							R"(,"op":"set","key":")" + JsonEscape(entry->iniKey) +
							R"(","wrote":")" + JsonEscape(a_command.value) +
							R"(","readback":")" + JsonEscape(after.raw) +
							R"(","source":")" + ValueSourceName(after.source) + R"("})";
					}
					break;

				case DriveCommand::Kind::Rebind:
					{
						if (entry->type != EntryType::Keymap) {
							a_command.result =
								R"({"ok":false,"error":"not a keymap control","key":")" +
								JsonEscape(entry->iniKey) + R"("})";
							break;
						}

						// Arm the row exactly as clicking Rebind does, supply the code the input
						// thread would have captured, then run the SAME ConsumeCapture the widget
						// runs - so a driven rebind proves the path a clicked one takes, rather than
						// a parallel copy of it that can drift.
						PageInput::BeginKeymapCapture();
						g_capturingKey = entry->iniKey;
						const bool injected =
							PageInput::InjectCapturedKey(a_command.code, a_command.cancel);

						std::uint32_t bound = 0;
						const CaptureOutcome outcome = ConsumeCapture(*panel, *entry, current, bound);

						const char* outcomeName = "unknown";
						switch (outcome) {
						case CaptureOutcome::NotPending:  outcomeName = "notPending"; break;
						case CaptureOutcome::Waiting:     outcomeName = "waiting"; break;
						case CaptureOutcome::Cancelled:   outcomeName = "cancelled"; break;
						case CaptureOutcome::Bound:       outcomeName = "bound"; break;
						case CaptureOutcome::Reserved:    outcomeName = "reserved"; break;
						case CaptureOutcome::InUse:       outcomeName = "inUse"; break;
						case CaptureOutcome::WriteFailed: outcomeName = "writeFailed"; break;
						}

						// A drive that neither bound nor cancelled would leave the row armed, and the
						// widget would sit in "press a key..." for ever. Clear it rather than leave
						// a failed request wedging the UI.
						if (outcome == CaptureOutcome::Waiting || outcome == CaptureOutcome::NotPending) {
							PageInput::CancelKeymapCapture();
							g_capturingKey.clear();
						}

						const auto after = store.Get(*panel, *entry);
						const bool ok = (outcome == CaptureOutcome::Bound ||
										 outcome == CaptureOutcome::Cancelled);
						// "consumed", NOT "injected". BeginCaptureFromInputThread returns true for
						// "capture owned this event", which includes REJECTING an unbindable code
						// (0 or kInvalid). Reporting that as injected=true asserted a key had been
						// supplied when it had been thrown away - a field a reader would trust
						// instead of checking the outcome, which is the only field that is the truth.
						a_command.result =
							std::string(R"({"ok":)") + (ok ? "true" : "false") +
							R"(,"op":"rebind","key":")" + JsonEscape(entry->iniKey) +
							R"(","outcome":")" + outcomeName +
							R"(","consumed":)" + (injected ? "true" : "false") +
							R"(,"code":)" + std::to_string(a_command.code) +
							R"(,"raw":")" + JsonEscape(after.raw) +
							R"(","source":")" + ValueSourceName(after.source) + R"("})";
					}
					break;

				default:
					a_command.result = R"({"ok":false,"error":"unhandled command"})";
					break;
				}
			}

			// Runs every queued request. Called from the TOP of Draw(), above the closed-page
			// early-out - below it, a request made while the page was shut would never run,
			// including the request to OPEN the page, which is the state the tool is used from.
			void DrainDriveQueue()
			{
				std::vector<std::shared_ptr<DriveCommand>> batch;
				{
					std::lock_guard<std::mutex> lock(g_driveMutex);
					if (g_driveQueue.empty()) {
						return;
					}
					batch.swap(g_driveQueue);
				}

				// Run OUTSIDE the lock: Set does load-modify-save file I/O, and holding the queue
				// lock across a disk write would stall every other tool call for its duration.
				for (auto& command : batch) {
					if (!command->abandoned) {
						RunDriveCommand(*command);
					}
					{
						std::lock_guard<std::mutex> lock(g_driveMutex);
						command->done = true;
					}
				}
				g_driveCv.notify_all();
			}

			bool SubmitDriveCommand(const std::shared_ptr<DriveCommand>& a_command,
				std::string& a_result, int a_timeoutMs)
			{
				{
					std::lock_guard<std::mutex> lock(g_driveMutex);
					g_driveQueue.push_back(a_command);
				}

				std::unique_lock<std::mutex> lock(g_driveMutex);
				if (!g_driveCv.wait_for(lock, std::chrono::milliseconds(a_timeoutMs),
						[&a_command] { return a_command->done; })) {
					// Mark it so the drain skips it. The caller has given up, and doing file I/O for
					// a result nobody will read is pure cost.
					a_command->abandoned = true;
					a_result =
						R"({"ok":false,"error":"timeout - the render thread did not run the request. Is the game drawing frames?"})";
					return false;
				}
				a_result = a_command->result;
				return true;
			}
		}

		void LoadStartupFlag()
		{
			// debug.ini already ships and is already read at startup for logging, so this adds a
			// key to a file that exists rather than a new file to the package.
			CSimpleIniA ini;
			ini.SetUnicode();
			if (ini.LoadFile(R"(Data\SKSE\Plugins\wheeler\debug.ini)") < 0) {
				return;
			}

			if (ini.GetBoolValue("SettingsPage", "ShowOnLoad", false)) {
				g_open = true;
				INFO("[SettingsPage] Page opened at load ([SettingsPage] ShowOnLoad = true).");
			}
		}

		bool IsOpen()
		{
			return g_open;
		}

		void SetOpen(bool a_open)
		{
			if (g_open == a_open) {
				return;
			}
			g_open = a_open;

			// Clears the queue and any half-held state on both edges, so a key held across the
			// transition cannot leak into the page - or out of it.
			if (g_open) {
				PageInput::OnPageOpened();
			} else {
				PageInput::OnPageClosed();
			}
		}

		void Toggle()
		{
			SetOpen(!g_open);
		}

		// ---- driving (DevBench) ----------------------------------------------------------

		std::string DriveStatusJson()
		{
			const auto& catalog = Catalog::GetSingleton();
			const auto& model = PageModel::GetSingleton();
			const bool ready = catalog.IsLoaded() && model.IsBuilt();

			std::string out = std::string(R"({"ok":true,"op":"status","open":)") +
				(g_open.load(std::memory_order_relaxed) ? "true" : "false") +
				R"(,"ready":)" + (ready ? "true" : "false") +
				R"(,"toggleKey":)" + std::to_string(Config::Control::Wheel::SettingsPageKey);

			if (ready) {
				const auto stats = model.Measure();
				out += R"(,"panels":)" + std::to_string(stats.panels) +
					R"(,"controlsPlaced":)" + std::to_string(stats.controlsPlaced);
			}

			// Wheel state, so a driven test has an OBSERVABLE for the input path. Without it a
			// rebound key could be pressed and nothing could be said about whether the dispatcher
			// had actually rebound - the stimulus existed and the readout did not, which is not a
			// test. These three are what Input.cpp's own spy records, read the same way.
			out += R"(,"wheelOpen":)" + std::string(Wheeler::IsWheelerOpen() ? "true" : "false") +
				R"(,"ammoWheelOpen":)" + std::string(Wheeler::IsAmmoWheelOpen() ? "true" : "false") +
				R"(,"editMode":)" + std::string(Wheeler::IsInEditMode() ? "true" : "false");

			out += "}";
			return out;
		}

		std::string DriveTabsJson()
		{
			const auto& model = PageModel::GetSingleton();
			if (!model.IsBuilt()) {
				return R"({"ok":false,"error":"the tab model is not built yet"})";
			}

			std::string out = R"({"ok":true,"op":"tabs","panels":[)";
			bool firstPanel = true;
			for (const auto& panelTab : model.Panels()) {
				if (!panelTab.panel) {
					continue;
				}
				if (!firstPanel) {
					out += ",";
				}
				firstPanel = false;
				out += std::string(R"({"label":")") + JsonEscape(panelTab.label) + R"(","tabs":[)";

				bool firstTab = true;
				for (const auto& tab : panelTab.tabs) {
					if (!firstTab) {
						out += ",";
					}
					firstTab = false;
					out += std::string(R"({"label":")") + JsonEscape(tab.label) +
						R"(","controls":)" + std::to_string(tab.DirectControlCount()) +
						R"(,"children":)" + std::to_string(static_cast<int>(tab.children.size())) + "}";
				}
				out += "]}";
			}
			out += "]}";
			return out;
		}

		std::string DriveListJson(const std::string& a_panelLabel, const std::string& a_tabLabel)
		{
			const auto& model = PageModel::GetSingleton();
			if (!model.IsBuilt()) {
				return R"({"ok":false,"error":"the tab model is not built yet"})";
			}

			for (const auto& panelTab : model.Panels()) {
				if (!panelTab.panel) {
					continue;
				}
				if (!a_panelLabel.empty() && panelTab.label != a_panelLabel) {
					continue;
				}
				for (const auto& tab : panelTab.tabs) {
					if (!a_tabLabel.empty() && tab.label != a_tabLabel) {
						continue;
					}
					return ListTabJson(panelTab.label, tab);
				}
			}
			return R"({"ok":false,"error":"no such panel or tab - call op=tabs to see what exists"})";
		}

		bool DriveSelectTab(const std::string& a_panelLabel, const std::string& a_tabLabel,
			std::string& a_resultJson, int a_timeoutMs)
		{
			auto command = std::make_shared<DriveCommand>();
			command->kind = DriveCommand::Kind::SelectTab;
			command->key = a_panelLabel;
			command->value = a_tabLabel;
			return SubmitDriveCommand(command, a_resultJson, a_timeoutMs);
		}

		bool DriveGet(const std::string& a_iniKey, std::string& a_resultJson, int a_timeoutMs)
		{
			auto command = std::make_shared<DriveCommand>();
			command->kind = DriveCommand::Kind::Get;
			command->key = a_iniKey;
			return SubmitDriveCommand(command, a_resultJson, a_timeoutMs);
		}

		bool DriveSet(const std::string& a_iniKey, const std::string& a_rawValue,
			std::string& a_resultJson, int a_timeoutMs)
		{
			auto command = std::make_shared<DriveCommand>();
			command->kind = DriveCommand::Kind::Set;
			command->key = a_iniKey;
			command->value = a_rawValue;
			return SubmitDriveCommand(command, a_resultJson, a_timeoutMs);
		}

		bool DriveRebind(const std::string& a_iniKey, std::uint32_t a_dispatchCode, bool a_cancel,
			std::string& a_resultJson, int a_timeoutMs)
		{
			auto command = std::make_shared<DriveCommand>();
			command->kind = DriveCommand::Kind::Rebind;
			command->key = a_iniKey;
			command->code = a_dispatchCode;
			command->cancel = a_cancel;
			return SubmitDriveCommand(command, a_resultJson, a_timeoutMs);
		}

		void Draw()
		{
			// Above the early-out on purpose - see DrainDriveQueue.
			DrainDriveQueue();

			if (!g_open) {
				return;
			}
			if (!Catalog::GetSingleton().IsLoaded() || !PageModel::GetSingleton().IsBuilt()) {
				return;
			}

			ImGui::SetNextWindowSize(ImVec2(980.0f, 640.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowBgAlpha(0.94f);

			// Begin wants a bool* for the title-bar close button, which an atomic cannot provide.
			// Mirroring it through a local also closes a real gap: passing &g_open wrote the flag
			// DIRECTLY, so clicking the X skipped SetOpen and therefore PageInput::OnPageClosed,
			// leaving the input queue and held-button state stale on the one close path that does
			// not go through the toggle key.
			bool stillOpen = g_open.load(std::memory_order_relaxed);

			if (ImGui::Begin("Wheeler Settings", &stillOpen)) {
				// Labels shown whole, the bar scrolling when they overflow, and a tab-list button so
				// every tab is reachable without scrolling - the house convention, not ImGui's
				// default shrink-to-fit.
				if (ImGui::BeginTabBar("panels",
						ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
					for (const auto& panelTab : PageModel::GetSingleton().Panels()) {
						if (!panelTab.panel) {
							continue;
						}
						// A pending selection is consumed the frame it is honoured, so a driven
						// selecttab MOVES the page once rather than pinning it there against the
						// player's own clicks for every later frame.
						ImGuiTabItemFlags panelFlags = ImGuiTabItemFlags_None;
						if (!g_pendingPanelTab.empty() && g_pendingPanelTab == panelTab.label) {
							panelFlags = ImGuiTabItemFlags_SetSelected;
							g_pendingPanelTab.clear();
						}
						if (ImGui::BeginTabItem(panelTab.label.c_str(), nullptr, panelFlags)) {
							// M7 - slots per wheel. A LIVE control, not a descriptor entry: it acts on the wheel
							// currently loaded (Wheeler::SetCurrentWheelSlotCount, tail-only shrink that refuses to
							// discard a filled slot), so it cannot be an INI value and lives above the Wheel
							// Behavior panel's tabs, where the wheel's structure is configured. The design call
							// (2026-09-12, auto sort mode): a slider 1..64 applied on release, mirroring the live
							// count whenever it is not being dragged; refusals arrive through the existing
							// notification path.
							if (panelTab.label.find("Wheel Behavior") != std::string::npos) {
								DrawLiveSlotCount();
							}
							if (ImGui::BeginTabBar("tabs",
									ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
								for (const auto& tab : panelTab.tabs) {
									ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
									if (!g_pendingTab.empty() && g_pendingTab == tab.label) {
										tabFlags = ImGuiTabItemFlags_SetSelected;
										g_pendingTab.clear();
									}
									if (ImGui::BeginTabItem(tab.label.c_str(), nullptr, tabFlags)) {
										ImGui::BeginChild("body", ImVec2(0, 0), false);
										if (panelTab.label.find("Wheeler Controls") != std::string::npos && &tab == &panelTab.tabs.front()) {
											DrawPresetsPanel();   // General tab, above the descriptor's own controls
										}
										DrawTab(*panelTab.panel, tab);
										ImGui::EndChild();
										ImGui::EndTabItem();
									}
								}
								ImGui::EndTabBar();
							}
							ImGui::EndTabItem();
						}
					}
					ImGui::EndTabBar();
				}
			}
			ImGui::End();

			// The title-bar close button, routed through SetOpen so the queue and held-button
			// state are reset exactly as they are for the toggle key.
			if (!stillOpen) {
				SetOpen(false);
			}

			// Sampled inside the frame, for the controller scheme: ImGui drives both navigation
			// and value-tweaking from the same nav axes, so exactly one stick may be wired to them
			// per frame - the left while nothing is held, the right once an item is taken hold of.
			// The input side cannot know which without being told from in here.
			PageInput::SetItemActive(ImGui::IsAnyItemActive());
		}

		// ---- M9: the framework-hosted page (AmfPage.cpp) reaches the shared logic through these ----
		namespace Shared
		{
			void BeginCapture(const std::string& a_iniKey)
			{
				PageInput::BeginKeymapCapture();
				g_capturingKey = a_iniKey;
				if (g_rowNoticeKey == a_iniKey) {
					g_rowNoticeKey.clear();
					g_rowNotice.clear();
				}
			}

			void CancelCapture()
			{
				PageInput::CancelKeymapCapture();
				g_capturingKey.clear();
			}

			bool IsCapturing(const std::string& a_iniKey)
			{
				return !a_iniKey.empty() && g_capturingKey == a_iniKey;
			}

			bool IsAnyCapturing()
			{
				return !g_capturingKey.empty();
			}

			CaptureOutcome ConsumeCapture(const Panel& a_panel, const Entry& a_entry,
				const ResolvedValue& a_current, std::uint32_t& a_boundCode)
			{
				switch (SettingsPage::Page::ConsumeCapture(a_panel, a_entry, a_current, a_boundCode)) {
				case SettingsPage::Page::CaptureOutcome::NotPending:  return CaptureOutcome::NotPending;
				case SettingsPage::Page::CaptureOutcome::Waiting:     return CaptureOutcome::Waiting;
				case SettingsPage::Page::CaptureOutcome::Cancelled:   return CaptureOutcome::Cancelled;
				case SettingsPage::Page::CaptureOutcome::Bound:       return CaptureOutcome::Bound;
				case SettingsPage::Page::CaptureOutcome::Reserved:    return CaptureOutcome::Reserved;
				case SettingsPage::Page::CaptureOutcome::InUse:       return CaptureOutcome::InUse;
				case SettingsPage::Page::CaptureOutcome::WriteFailed: return CaptureOutcome::WriteFailed;
				}
				return CaptureOutcome::NotPending;
			}

			std::string RowNotice(const std::string& a_iniKey)
			{
				return (!a_iniKey.empty() && g_rowNoticeKey == a_iniKey) ? g_rowNotice : std::string{};
			}

			void ClearRowNotice(const std::string& a_iniKey)
			{
				if (g_rowNoticeKey == a_iniKey) {
					g_rowNoticeKey.clear();
					g_rowNotice.clear();
				}
			}

			bool LooksLikeGamepad(const Entry& a_entry)
			{
				return SettingsPage::Page::LooksLikeGamepad(a_entry);
			}

			bool IsKnownButton(const std::string& a_id)
			{
				return IsKnownButtonId(a_id);
			}

			void RunButtonAction(const std::string& a_id)
			{
				SettingsPage::Page::RunButtonAction(a_id);
			}

			void ApplyChangedSettings()
			{
				SettingsPage::Page::ApplyChangedSettings();
			}
		}
	}
}
