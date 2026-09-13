#include "SettingsPresets.h"

#include "bin/Config.h"
#include "bin/SettingsPage/Descriptor.h"
#include "bin/SettingsPage/PageShared.h"
#include "bin/SettingsPage/ValueStore.h"
#include "bin/Texts.h"
#include "bin/UserInput/Controls.h"
#include "bin/Utilities/Utils.h"
#include "bin/Wheeler/AmmoWheelReskinUnified.h"
#include "bin/Wheeler/Wheeler.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <system_error>

namespace SettingsPresets
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kRoot = "Data\\SKSE\\Plugins\\wheeler\\user\\presets";
		constexpr std::size_t kMaxNameLength = 40;

		// The seven settings INIs the descriptors target. The resolution-specific layout files
		// (user\MainWheel.Layout.ini) and the bridge's generated layout are NOT settings and stay out:
		// a preset moved to another screen would otherwise carry the wrong pixel layout with it.
		const std::vector<IniFile> kFiles = {
			{ "Styles", "Data\\SKSE\\Plugins\\wheeler\\Styles.ini", "Data\\SKSE\\Plugins\\wheeler\\Styles.defaults.ini" },
			{ "I4", "Data\\SKSE\\Plugins\\wheeler\\I4.ini", "Data\\SKSE\\Plugins\\wheeler\\I4.defaults.ini" },
			{ "WheelBehavior", "Data\\SKSE\\Plugins\\wheeler\\wheelBehavior.ini", "Data\\SKSE\\Plugins\\wheeler\\wheelBehavior.factory.ini" },
			{ "Controls", "Data\\SKSE\\Plugins\\wheeler\\Controls.ini", "Data\\SKSE\\Plugins\\wheeler\\Controls.defaults.ini" },
			{ "ActionHotkeysBridge", "Data\\SKSE\\Plugins\\wheeler\\ActionHotkeysBridge.ini", "Data\\SKSE\\Plugins\\wheeler\\ActionHotkeysBridge.defaults.ini" },
			{ "OStimIntegration", "Data\\SKSE\\Plugins\\wheeler\\OStimIntegration.ini", "Data\\SKSE\\Plugins\\wheeler\\OStimIntegration.defaults.ini" },
			{ "AmmoWheel", "Data\\SKSE\\Plugins\\wheeler\\AmmoWheel.ini", "Data\\SKSE\\Plugins\\wheeler\\AmmoWheel.defaults.ini" },
		};

		std::mutex g_lock;
		std::string g_savePreset;   // the loaded save's preset name
		std::string g_lastApplied;  // the preset whose files were last copied over the live INIs
		std::string g_notice;
		std::array<char, 64> g_nameBuffer{};

		std::string Trim(std::string a_s)
		{
			while (!a_s.empty() && std::isspace(static_cast<unsigned char>(a_s.back()))) { a_s.pop_back(); }
			std::size_t i = 0;
			while (i < a_s.size() && std::isspace(static_cast<unsigned char>(a_s[i]))) { ++i; }
			return a_s.substr(i);
		}

		std::string Lower(std::string a_s)
		{
			std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		fs::path PresetDir(const std::string& a_name)
		{
			return fs::path(kRoot) / a_name;
		}

		std::string FileName(const char* a_path)
		{
			return fs::path(a_path).filename().string();
		}

		std::string Escape(const std::string& a_s)
		{
			std::string out;
			for (char c : a_s) {
				if (c == '"' || c == '\\') { out += '\\'; }
				out += c;
			}
			return out;
		}

		// The ammo wheel's key-name mirrors are derived values; the factory-restore button recomputes
		// them after a reload and so does every path here that replaces AmmoWheel.ini.
		void RefreshAmmoWheelKeyNames()
		{
			Config::AmmoWheel::ToggleKeyMKBName = Controls::GetKeyNameForMkb(Config::AmmoWheel::MKB::toggleAmmoWheel);
			Config::AmmoWheel::ToggleKeyGamepadName = Controls::GetKeyNameForGamepad(Config::AmmoWheel::GamePad::toggleAmmoWheel);
			Config::AmmoWheel::ModifierKeyMKBName = Config::AmmoWheel::MKB::modifierKey ?
				Controls::GetKeyNameForMkb(Config::AmmoWheel::MKB::modifierKey) : "None";
			Config::AmmoWheel::ModifierButtonGamepadName = Config::AmmoWheel::GamePad::modifierButton ?
				Controls::GetKeyNameForGamepad(Config::AmmoWheel::GamePad::modifierButton) : "None";
			Config::AmmoWheel::ToggleMouseButtonName = Config::AmmoWheel::MKB::toggleAmmoWheelMouse ?
				Controls::GetKeyNameForMkb(Config::AmmoWheel::MKB::toggleAmmoWheelMouse) : "None";
		}

		bool CopyOver(const fs::path& a_from, const fs::path& a_to, std::string& a_err)
		{
			std::error_code ec;
			fs::create_directories(a_to.parent_path(), ec);
			ec.clear();
			if (!fs::copy_file(a_from, a_to, fs::copy_options::overwrite_existing, ec) || ec) {
				a_err = "copy failed: " + a_from.string() + " -> " + a_to.string() + " (" + ec.message() + ")";
				logger::error("[SettingsPresets] {}", a_err);
				return false;
			}
			return true;
		}
	}

	const std::vector<IniFile>& Files() { return kFiles; }
	const char* Root() { return kRoot; }

	std::vector<std::string> List()
	{
		std::vector<std::string> names;
		std::error_code ec;
		if (!fs::exists(kRoot, ec) || ec) {
			return names;
		}
		for (const auto& entry : fs::directory_iterator(kRoot, ec)) {
			if (entry.is_directory(ec)) {
				names.push_back(entry.path().filename().string());
			}
		}
		std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return Lower(a) < Lower(b); });
		return names;
	}

	bool Exists(const std::string& a_name)
	{
		std::string reason;
		if (!IsValidName(a_name, reason)) {
			return false;
		}
		std::error_code ec;
		return fs::is_directory(PresetDir(a_name), ec) && !ec;
	}

	bool IsValidName(const std::string& a_name, std::string& a_reason)
	{
		const std::string name = Trim(a_name);
		if (name.empty()) {
			a_reason = Texts::GetText(Texts::TextType::PresetInvalidName);
			return false;
		}
		if (name.size() > kMaxNameLength || name == "." || name == ".." || name.front() == '.') {
			a_reason = Texts::GetText(Texts::TextType::PresetInvalidName);
			return false;
		}
		for (unsigned char c : name) {
			const bool ok = std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '(' || c == ')' || c == '.';
			if (!ok) {
				a_reason = Texts::GetText(Texts::TextType::PresetInvalidName);
				return false;
			}
		}
		return true;
	}

	bool SaveAs(const std::string& a_rawName, std::string& a_err)
	{
		const std::string name = Trim(a_rawName);
		if (!IsValidName(name, a_err)) {
			return false;
		}
		const fs::path dir = PresetDir(name);
		std::error_code ec;
		fs::create_directories(dir, ec);
		if (ec) {
			a_err = "cannot create " + dir.string() + " (" + ec.message() + ")";
			logger::error("[SettingsPresets] {}", a_err);
			return false;
		}
		int copied = 0;
		for (const auto& f : kFiles) {
			// A live INI that was never bootstrapped is the shipped defaults by definition, so the
			// preset takes the defaults file for it and stays complete.
			ec.clear();
			const char* source = (fs::exists(f.userPath, ec) && !ec) ? f.userPath : f.defaultsPath;
			ec.clear();
			if (!fs::exists(source, ec) || ec) {
				logger::warn("[SettingsPresets] save '{}': neither '{}' nor '{}' exists; skipped", name, f.userPath, f.defaultsPath);
				continue;
			}
			if (!CopyOver(source, dir / FileName(f.userPath), a_err)) {
				return false;
			}
			++copied;
		}
		{
			std::scoped_lock l(g_lock);
			g_lastApplied = name;   // the live settings and the preset are now identical
		}
		logger::info("[SettingsPresets] saved preset '{}' ({} files) to {}", name, copied, dir.string());
		return true;
	}

	bool Load(const std::string& a_rawName, std::string& a_err)
	{
		const std::string name = Trim(a_rawName);
		if (!IsValidName(name, a_err)) {
			return false;
		}
		if (!Exists(name)) {
			a_err = std::string(Texts::GetText(Texts::TextType::PresetNotFound)) + " " + name;
			return false;
		}
		const fs::path dir = PresetDir(name);
		int copied = 0;
		for (const auto& f : kFiles) {
			const fs::path source = dir / FileName(f.userPath);
			std::error_code ec;
			if (!fs::exists(source, ec) || ec) {
				continue;   // an older preset without this file leaves the live one alone
			}
			if (!CopyOver(source, f.userPath, a_err)) {
				return false;
			}
			++copied;
		}
		ReloadFromIni();
		{
			std::scoped_lock l(g_lock);
			g_lastApplied = name;
		}
		logger::info("[SettingsPresets] loaded preset '{}' ({} files) from {}", name, copied, dir.string());
		return true;
	}

	bool Rename(const std::string& a_rawFrom, const std::string& a_rawTo, std::string& a_err)
	{
		const std::string from = Trim(a_rawFrom);
		const std::string to = Trim(a_rawTo);
		if (!IsValidName(from, a_err) || !IsValidName(to, a_err)) {
			return false;
		}
		if (!Exists(from)) {
			a_err = std::string(Texts::GetText(Texts::TextType::PresetNotFound)) + " " + from;
			return false;
		}
		if (Lower(from) == Lower(to)) {
			a_err = Texts::GetText(Texts::TextType::PresetInvalidName);
			return false;
		}
		if (Exists(to)) {
			a_err = std::string(Texts::GetText(Texts::TextType::PresetAlreadyExists)) + " " + to;
			return false;
		}
		std::error_code ec;
		fs::rename(PresetDir(from), PresetDir(to), ec);
		if (ec) {
			a_err = "rename failed (" + ec.message() + ")";
			logger::error("[SettingsPresets] {}", a_err);
			return false;
		}
		{
			std::scoped_lock l(g_lock);
			if (Lower(g_savePreset) == Lower(from)) { g_savePreset = to; }
			if (Lower(g_lastApplied) == Lower(from)) { g_lastApplied = to; }
		}
		logger::info("[SettingsPresets] renamed preset '{}' -> '{}'", from, to);
		return true;
	}

	bool Delete(const std::string& a_rawName, std::string& a_err)
	{
		const std::string name = Trim(a_rawName);
		if (!IsValidName(name, a_err)) {
			return false;
		}
		if (!Exists(name)) {
			a_err = std::string(Texts::GetText(Texts::TextType::PresetNotFound)) + " " + name;
			return false;
		}
		std::error_code ec;
		fs::remove_all(PresetDir(name), ec);
		if (ec) {
			a_err = "delete failed (" + ec.message() + ")";
			logger::error("[SettingsPresets] {}", a_err);
			return false;
		}
		{
			std::scoped_lock l(g_lock);
			if (Lower(g_savePreset) == Lower(name)) { g_savePreset.clear(); }
			if (Lower(g_lastApplied) == Lower(name)) { g_lastApplied.clear(); }
		}
		logger::info("[SettingsPresets] deleted preset '{}'", name);
		return true;
	}

	bool ResetToDefaults(std::string& a_err)
	{
		int copied = 0;
		for (const auto& f : kFiles) {
			std::error_code ec;
			if (!fs::exists(f.defaultsPath, ec) || ec) {
				logger::warn("[SettingsPresets] reset: defaults file '{}' is missing; '{}' left as it is", f.defaultsPath, f.userPath);
				continue;
			}
			if (!CopyOver(f.defaultsPath, f.userPath, a_err)) {
				return false;
			}
			++copied;
		}
		ReloadFromIni();
		{
			std::scoped_lock l(g_lock);
			g_lastApplied.clear();   // the live settings no longer match any preset
		}
		logger::info("[SettingsPresets] reset {} settings files to their shipped defaults", copied);
		return true;
	}

	// The same reload the settings page runs after a write, plus the two consumers a whole-file
	// replacement also invalidates (the ammo reskin reads its INIs itself; the wheel is told the
	// ammo config changed) and the derived key names.
	void ReloadFromIni()
	{
		SettingsPage::ValueStore::GetSingleton().LoadAll(SettingsPage::Catalog::GetSingleton());
		SettingsPage::Page::Shared::ApplyChangedSettings();
		AmmoWheelReskinUnified::ReskinSystem::GetSingleton().ReloadSmartFromIni();
		RefreshAmmoWheelKeyNames();
		Wheeler::NotifyAmmoWheelConfigChanged();
		logger::info("[SettingsPresets] settings reloaded from the INI files");
	}

	void SetSavePreset(const std::string& a_name)
	{
		std::scoped_lock l(g_lock);
		g_savePreset = Trim(a_name);
	}

	std::string SavePreset()
	{
		std::scoped_lock l(g_lock);
		return g_savePreset;
	}

	void ClearSavePreset()
	{
		std::scoped_lock l(g_lock);
		g_savePreset.clear();
	}

	std::string LastApplied()
	{
		std::scoped_lock l(g_lock);
		return g_lastApplied;
	}

	void ApplySavePresetOnLoad(const char* a_reason)
	{
		const std::string name = SavePreset();
		if (name.empty()) {
			logger::debug("[SettingsPresets] {}: the save names no preset; settings unchanged", a_reason ? a_reason : "load");
			return;
		}
		if (!Exists(name)) {
			logger::warn("[SettingsPresets] {}: the save names preset '{}' but no such folder exists under {}; settings unchanged", a_reason ? a_reason : "load", name, kRoot);
			SetNotice(std::string(Texts::GetText(Texts::TextType::PresetNotFound)) + " " + name);
			return;
		}
		if (Lower(LastApplied()) == Lower(name)) {
			logger::info("[SettingsPresets] {}: preset '{}' is already in force; settings kept as they are", a_reason ? a_reason : "load", name);
			return;
		}
		std::string err;
		if (Load(name, err)) {
			const std::string msg = std::string(Texts::GetText(Texts::TextType::PresetAppliedForSave)) + " " + name;
			SetNotice(msg);
			Utils::NotificationMessage(msg);
		} else {
			SetNotice(err);
		}
	}

	char* NameBuffer() { return g_nameBuffer.data(); }
	std::size_t NameBufferSize() { return g_nameBuffer.size(); }

	std::string Notice()
	{
		std::scoped_lock l(g_lock);
		return g_notice;
	}

	void SetNotice(std::string a_notice)
	{
		std::scoped_lock l(g_lock);
		g_notice = std::move(a_notice);
	}

	std::string StatusJson()
	{
		std::string list;
		for (const auto& n : List()) {
			if (!list.empty()) { list += ","; }
			list += "\"" + Escape(n) + "\"";
		}
		return "\"presets\":[" + list + "],\"savePreset\":\"" + Escape(SavePreset()) + "\",\"lastApplied\":\"" + Escape(LastApplied()) +
		       "\",\"notice\":\"" + Escape(Notice()) + "\",\"root\":\"" + Escape(kRoot) + "\"";
	}
}
