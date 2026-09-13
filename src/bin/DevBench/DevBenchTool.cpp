#include "bin/DevBench/DevBenchTool.h"
#include "bin/Texts.h"
#include "bin/SettingsPresets.h"

#include "bin/Wheeler/Wheeler.h"
#include "bin/DevBench/InputInject.h"
#include "bin/Config.h"
#include "bin/AMF/AMFLaunch.h"
#include "bin/AMF/AmfPage.h"
#include "bin/SettingsPage/PageShared.h"
#include "bin/DevBench/DevBenchAPI.h"
#include "bin/SettingsPage/Page.h"

#include <cstdlib>
#include <string>

namespace DevBenchTool
{
	namespace
	{
		// Minimal extractors for this tool's small, controlled argument shape. The same approach the
		// other driving tools in this project take: a JSON library is not worth a dependency for
		// reading three top-level fields, and the descriptor below fixes what those fields are.
		std::string JsonStr(const std::string& a_json, const char* a_key)
		{
			const std::string needle = std::string("\"") + a_key + "\"";
			auto pos = a_json.find(needle);
			if (pos == std::string::npos) {
				return "";
			}
			pos = a_json.find(':', pos + needle.size());
			if (pos == std::string::npos) {
				return "";
			}
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) {
				++pos;
			}
			if (pos >= a_json.size() || a_json[pos] != '"') {
				return "";
			}
			++pos;
			std::string out;
			while (pos < a_json.size() && a_json[pos] != '"') {
				if (a_json[pos] == '\\' && pos + 1 < a_json.size()) {
					++pos;
				}
				out += a_json[pos];
				++pos;
			}
			return out;
		}

		double JsonNum(const std::string& a_json, const char* a_key, double a_default)
		{
			const std::string needle = std::string("\"") + a_key + "\"";
			auto pos = a_json.find(needle);
			if (pos == std::string::npos) {
				return a_default;
			}
			pos = a_json.find(':', pos + needle.size());
			if (pos == std::string::npos) {
				return a_default;
			}
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) {
				++pos;
			}
			try {
				return std::stod(a_json.substr(pos));
			} catch (...) {
				return a_default;
			}
		}

		// Reads the token immediately after the key's colon, rather than searching for "true"
		// somewhere later in the document. The searching version answers correctly for the shapes
		// this tool expects but misreads a false value whose LATER sibling is true, which is the
		// kind of wrong answer that would look like the cancel flag being ignored.
		bool JsonBool(const std::string& a_json, const char* a_key)
		{
			const std::string needle = std::string("\"") + a_key + "\"";
			auto pos = a_json.find(needle);
			if (pos == std::string::npos) {
				return false;
			}
			pos = a_json.find(':', pos + needle.size());
			if (pos == std::string::npos) {
				return false;
			}
			++pos;
			while (pos < a_json.size() && (a_json[pos] == ' ' || a_json[pos] == '\t')) {
				++pos;
			}
			return (pos + 4 <= a_json.size()) && a_json.compare(pos, 4, "true") == 0;
		}

		// NOTE the JSON delimiter. A plain R"( ... )" ends at the first `)"` sequence, and this
		// descriptor's own help text contains four of them - "(get, set, rebind)", "(set)",
		// "(list, selecttab)", "(rebind)" - each closing paren sitting immediately before the JSON
		// string's closing quote. With the default delimiter the literal terminated mid-descriptor
		// and the rest of the file parsed as garbage.
		constexpr const char* kDescriptor = R"JSON({
"description":"Drive Wheeler's in-game settings page: open or close it, move to a tab, read and write any setting by its INI key, and run a keymap rebind end to end without a physical key press. Setting keys may be given as 'Key' or, when a bare key is ambiguous across the eight descriptor files, as 'Section/Key'.",
"inputSchema":{"type":"object","properties":{
"op":{"type":"string","enum":["status","open","close","toggle","tabs","list","get","set","rebind","selecttab"],"description":"what to do"},
"key":{"type":"string","description":"setting INI key, 'Key' or 'Section/Key' (get, set, rebind)"},
"value":{"type":"string","description":"raw value to write, in the notation the INI already uses (set)"},
"panel":{"type":"string","description":"panel tab label (list, selecttab)"},
"tab":{"type":"string","description":"tab label within the panel (list, selecttab)"},
"code":{"type":"number","description":"post-offset dispatch code to bind: keyboard is the raw DIK scan code, mouse is +256, gamepad is an index above 266 (rebind)"},
"cancel":{"type":"boolean","description":"exercise the cancel branch instead of binding (rebind)"}
},"required":["op"]},
"readOnly":false})JSON";

		// Runs on devbench's LISTENER thread. Everything that touches render-thread state goes
		// through Page's Drive* entry points, which queue the request and wait for the render thread;
		// the read-only ops answer straight from the Catalog and PageModel, which are never mutated
		// after load.
		void PageTool(void*, const char* a_argsJson, void* a_sink, DevBenchAPI::WriteFn a_write)
		{
			const std::string args = a_argsJson ? a_argsJson : "{}";
			const std::string op = JsonStr(args, "op");

			std::string result;

			if (op.empty() || op == "status") {
				result = SettingsPage::Page::DriveStatusJson();
			} else if (op == "open" || op == "close" || op == "toggle") {
				if (op == "toggle") {
					SettingsPage::Page::Toggle();
				} else {
					SettingsPage::Page::SetOpen(op == "open");
				}
				result = SettingsPage::Page::DriveStatusJson();
			} else if (op == "tabs") {
				result = SettingsPage::Page::DriveTabsJson();
			} else if (op == "list") {
				result = SettingsPage::Page::DriveListJson(JsonStr(args, "panel"), JsonStr(args, "tab"));
			} else if (op == "get") {
				SettingsPage::Page::DriveGet(JsonStr(args, "key"), result);
			} else if (op == "set") {
				SettingsPage::Page::DriveSet(JsonStr(args, "key"), JsonStr(args, "value"), result);
			} else if (op == "rebind") {
				const auto code = static_cast<std::uint32_t>(JsonNum(args, "code", 0.0));
				SettingsPage::Page::DriveRebind(JsonStr(args, "key"), code, JsonBool(args, "cancel"), result);
			} else if (op == "selecttab") {
				SettingsPage::Page::DriveSelectTab(JsonStr(args, "panel"), JsonStr(args, "tab"), result);
			} else if (op == "inject") {
				// A press spliced ahead of Wheeler's own filter: device keyboard|gamepad|mouse, code, hold (frames).
				const std::string dev = JsonStr(args, "device");
				const std::uint32_t d = dev == "gamepad" ? 2u : (dev == "mouse" ? 1u : 0u);
				const auto code = static_cast<std::uint32_t>(JsonNum(args, "code", 0.0));
				const int hold = static_cast<int>(JsonNum(args, "hold", 4.0));
				if (code == 0) {
					result = R"({"ok":false,"error":"inject needs a non-zero code"})";
				} else {
					InputInject::QueuePress(d, code, hold);
					result = "{\"ok\":true,\"op\":\"inject\",\"device\":\"" + dev + "\",\"code\":" + std::to_string(code) + ",\"hold\":" + std::to_string(hold) + ",\"queued\":" + std::to_string(InputInject::Pending()) + "}";
				}
			} else if (op == "slots") {
				// M7: read or set the loaded wheel's slot count through the same setter the slider uses.
				if (JsonBool(args, "create") && Wheeler::GetCurrentWheelSlotCount() < 0) { Wheeler::PushWheel(); }   // the page's 'Create the first wheel'
				const int before = Wheeler::GetCurrentWheelSlotCount();
				const int desired = static_cast<int>(JsonNum(args, "value", -1.0));
				int after = before;
				if (desired > 0 && before >= 0) { after = Wheeler::SetCurrentWheelSlotCount(desired); }   // a fresh wheel has 0 entries and is still a wheel
				result = "{\"ok\":true,\"op\":\"slots\",\"before\":" + std::to_string(before) + ",\"requested\":" + std::to_string(desired) + ",\"after\":" + std::to_string(after) + "}";
			} else if (op == "wheels") {
				// The number-of-wheels slider's setter (the owner, 2026-09-13). value = desired count.
				const int before = Wheeler::GetWheelCount();
				const int desired = static_cast<int>(JsonNum(args, "value", -1.0));
				int after = before;
				if (desired > 0) { after = Wheeler::SetWheelCount(desired); }
				result = "{\"ok\":true,\"op\":\"wheels\",\"before\":" + std::to_string(before) + ",\"requested\":" + std::to_string(desired) + ",\"after\":" + std::to_string(after) + "}";
			} else if (op == "presets") {
				// Settings presets (the owner, 2026-09-13). sub = list | save | load | rename | delete |
				// reset | reload | select; name / to as the sub needs. The result carries the
				// list, the selected preset and the last applied one, so a proof reads the state it just set.
				const std::string sub = JsonStr(args, "sub");
				const std::string name = JsonStr(args, "name");
				const std::string to = JsonStr(args, "to");
				bool ok = true;
				std::string err;
				if (sub == "save") { ok = SettingsPresets::SaveAs(name, err); if (ok) { SettingsPresets::SetSelected(name); } }
				else if (sub == "load") { ok = SettingsPresets::Load(name, err); if (ok) { SettingsPresets::SetSelected(name); } }
				else if (sub == "rename") { ok = SettingsPresets::Rename(name, to, err); }
				else if (sub == "delete") { ok = SettingsPresets::Delete(name, err); }
				else if (sub == "reset") { ok = SettingsPresets::ResetToDefaults(err); }
				else if (sub == "reload") { SettingsPresets::ReloadFromIni(); }
				else if (sub == "select") { SettingsPresets::SetSelected(name); }
				else if (!sub.empty() && sub != "list" && sub != "status") { ok = false; err = "unknown sub '" + sub + "'"; }
				std::string escErr; for (char c : err) { if (c == '"' || c == '\\') { escErr += '\\'; } escErr += c; }
				result = std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"op\":\"presets\",\"sub\":\"" + sub + "\",\"error\":\"" + escErr + "\"," + SettingsPresets::StatusJson() + "}";
			} else if (op == "texts") {
				// 1.1.2: which language is in force and from which file; lang=<name> reloads the strings
				// with that language (a proof reads a translated key back through 'sample').
				const std::string lang = JsonStr(args, "lang");
				if (!lang.empty()) { Texts::LoadLanguageFile(lang); }
				std::string esc; for (char c : Texts::LanguageFile()) { if (c == '\\' || c == '"') { esc += '\\'; } esc += c; }
				std::string sample; for (char c : std::string(Texts::GetText(Texts::TextType::PresetsHeader))) { if (c == '\\' || c == '"') { sample += '\\'; } sample += c; }
				result = "{\"ok\":true,\"op\":\"texts\",\"language\":\"" + Texts::Language() + "\",\"file\":\"" + esc +
					"\",\"applied\":" + std::to_string(Texts::LanguageEntries()) + ",\"sample\":\"" + sample + "\"}";
			} else if (op == "spy") {
				// Runtime switch for the input spy and the menu-block reasons, so a proof can read every
				// event's verdict from wheeler.log without depending on INI layering. sub=on|off|status.
				const std::string sub = JsonStr(args, "sub");
				if (sub == "on") { Config::Debug::InputSpy = true; Config::Debug::LogMenuBlockReasons = true; }
				else if (sub == "off") { Config::Debug::InputSpy = false; Config::Debug::LogMenuBlockReasons = false; }
				result = std::string("{\"ok\":true,\"op\":\"spy\",\"inputSpy\":") + (Config::Debug::InputSpy ? "true" : "false") +
					",\"logMenuBlockReasons\":" + (Config::Debug::LogMenuBlockReasons ? "true" : "false") +
					",\"rateLimitMs\":" + std::to_string(Config::Debug::InputSpyRateLimitMs) + "}";
			} else if (op == "amf") {
				// M3 observability: is the page registered with a menu framework, is a launch armed, and
				// where did AMF last draw the button (so amf.menu click can press the real one).
				// sub=arm does what the button does, for a rig with no framework menu open.
				const std::string sub = JsonStr(args, "sub");
				if (sub == "arm") {
					// M9: arm a keymap row's capture the way the framework page's Rebind button does
					// (args key = "Section/Key"); the next real press through Wheeler's hook binds it.
					const std::string key = JsonStr(args, "key");
					const auto slash = key.find('/');
					SettingsPage::Page::Shared::BeginCapture(slash == std::string::npos ? key : key.substr(slash + 1));
				} else if (sub == "cancel") {
					SettingsPage::Page::Shared::CancelCapture();
				}
				const auto r = AMFLaunch::GetButtonRect();
				result = std::string("{\"ok\":true,\"op\":\"amf\",\"hosted\":") + (AmfPage::IsHosted() ? "true" : "false") +
					",\"hostedBy\":\"" + AmfPage::HostedBy() + "\",\"capturing\":" + (SettingsPage::Page::Shared::IsAnyCapturing() ? "true" : "false") +
					",\"registeredWith\":\"" + AMFLaunch::RegisteredWith() +
					"\",\"launchPending\":" + (AMFLaunch::IsLaunchPending() ? "true" : "false") +
					",\"pageOpen\":" + (SettingsPage::Page::IsOpen() ? "true" : "false") +
					",\"buttonRect\":[" + std::to_string(r.x0) + "," + std::to_string(r.y0) + "," + std::to_string(r.x1) + "," + std::to_string(r.y1) + "]}";
			} else {
				result =
					R"({"ok":false,"error":"unknown op","ops":["status","open","close","toggle","tabs","list","get","set","rebind","selecttab","amf","spy","inject","slots"]})";
			}

			a_write(a_sink, result.c_str());
		}

		bool g_registered = false;
	}

	void Init(bool a_lastAttempt)
	{
		if (g_registered) {
			return;
		}

		auto* devbench = DevBenchAPI::GetDevBenchInterface001();
		if (!devbench) {
			if (a_lastAttempt) {
				INFO("[SettingsPage] devbench not present - the wheeler.page driving tool is unavailable");
			}
			return;
		}

		g_registered = devbench->RegisterTool("wheeler.page", kDescriptor, PageTool, nullptr);
		INFO("[SettingsPage] devbench build {}: wheeler.page registered = {}",
			devbench->GetBuildNumber(), g_registered);
	}
}
