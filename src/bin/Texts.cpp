#include "Texts.h"
#include "bin/Rendering/RenderManager.h"

#include <fstream>
#include <algorithm>
#include <Windows.h>

namespace
{
	static void loadTextFromIni(CSimpleIniA& a_ini, const char* key, std::string& r_text)
	{
		const char* val = a_ini.GetValue("Texts", key, r_text.data());
		if (val) {
			r_text = std::string(val);
		}
	}

	std::string trim(const std::string& s)
	{
		auto begin = std::find_if_not(s.begin(), s.end(), [](unsigned char ch) { return std::isspace(ch); });
		auto end = std::find_if_not(s.rbegin(), s.rend(), [](unsigned char ch) { return std::isspace(ch); }).base();
		if (begin >= end) {
			return {};
		}
		return std::string(begin, end);
	}

	const char* GetKeyForTextType(Texts::TextType type)
	{
		switch (type) {
		case Texts::TextType::AlchemyDynamicIDConsumptionWarning:
			return "AlchemyDynamicIDConsumptionWarning";
		case Texts::TextType::NoWheelPresent:
			return "NoWheelPresent";
		case Texts::TextType::EditHintTitle:
			return "EditHintTitle";
		case Texts::TextType::EditHintDeviceHeaderMkb:
			return "EditHintDeviceHeaderMkb";
		case Texts::TextType::EditHintDeviceHeaderGamepad:
			return "EditHintDeviceHeaderGamepad";
		case Texts::TextType::EditHintActionUsePlaceItem:
			return "EditHintActionUsePlaceItem";
		case Texts::TextType::EditHintActionRemoveItemWheel:
			return "EditHintActionRemoveItemWheel";
		case Texts::TextType::EditHintActionAddEmptySlot:
			return "EditHintActionAddEmptySlot";
		case Texts::TextType::EditHintActionAddWheel:
			return "EditHintActionAddWheel";
		case Texts::TextType::EditHintActionNextWheel:
			return "EditHintActionNextWheel";
		case Texts::TextType::EditHintActionPreviousWheel:
			return "EditHintActionPreviousWheel";
		case Texts::TextType::EditHintActionMoveSlotForward:
			return "EditHintActionMoveSlotForward";
		case Texts::TextType::EditHintActionMoveSlotBack:
			return "EditHintActionMoveSlotBack";
		case Texts::TextType::EditHintActionPickUpSlot:
			return "EditHintActionPickUpSlot";
		case Texts::TextType::FavoriteAddedToSlot:
			return "FavoriteAddedToSlot";
		case Texts::TextType::FavoriteSlotFull:
			return "FavoriteSlotFull";
		case Texts::TextType::WheelNameInventory:
			return "WheelNameInventory";
		case Texts::TextType::WheelNameMagic:
			return "WheelNameMagic";
		case Texts::TextType::EditHintActionMoveWheelForward:
			return "EditHintActionMoveWheelForward";
		case Texts::TextType::EditHintActionMoveWheelBack:
			return "EditHintActionMoveWheelBack";
		case Texts::TextType::EditHintActionSettingsDMenu:
			return "EditHintActionSettingsDMenu";
		case Texts::TextType::AmfLaunchExplain:
			return "AmfLaunchExplain";
		case Texts::TextType::AmfLaunchButton:
			return "AmfLaunchButton";
		case Texts::TextType::AmfLaunchPending:
			return "AmfLaunchPending";
		case Texts::TextType::SlotCountLabel:
			return "SlotCountLabel";
		case Texts::TextType::SlotCountHelp:
			return "SlotCountHelp";
		case Texts::TextType::SlotCountNoWheel:
			return "SlotCountNoWheel";
		case Texts::TextType::SlotCountCreateFirst:
			return "SlotCountCreateFirst";
		case Texts::TextType::EditHintActionExitWheel:
			return "EditHintActionExitWheel";
		case Texts::TextType::EditHintNavToggleMkb:
			return "EditHintNavToggleMkb";
		case Texts::TextType::EditHintNavToggleGamepad:
			return "EditHintNavToggleGamepad";
		case Texts::TextType::WheelBehaviorDefaultsSaved:
			return "WheelBehaviorDefaultsSaved";
		case Texts::TextType::WheelBehaviorRestoredToDefaults:
			return "WheelBehaviorRestoredToDefaults";
		case Texts::TextType::NoWheelBehaviorDefaultsSaved:
			return "NoWheelBehaviorDefaultsSaved";
		case Texts::TextType::PressKeyToBindAmmoWheel:
			return "PressKeyToBindAmmoWheel";
		case Texts::TextType::PressGamepadButtonToBindAmmoWheel:
			return "PressGamepadButtonToBindAmmoWheel";
		case Texts::TextType::KeybindCaptureCancelled:
			return "KeybindCaptureCancelled";
		case Texts::TextType::AmmoWheelKeybindsReset:
			return "AmmoWheelKeybindsReset";
		case Texts::TextType::PressKeyToSetModifier:
			return "PressKeyToSetModifier";
		case Texts::TextType::KeyboardModifierCleared:
			return "KeyboardModifierCleared";
		case Texts::TextType::PressGamepadButtonToSetModifier:
			return "PressGamepadButtonToSetModifier";
		case Texts::TextType::GamepadModifierCleared:
			return "GamepadModifierCleared";
		case Texts::TextType::ClickMouseButtonToBind:
			return "ClickMouseButtonToBind";
		case Texts::TextType::MouseToggleCleared:
			return "MouseToggleCleared";
		case Texts::TextType::AmmoWheelFactoryDefaultsRestored:
			return "AmmoWheelFactoryDefaultsRestored";
		case Texts::TextType::AmmoWheelFactoryDefaultsFailed:
			return "AmmoWheelFactoryDefaultsFailed";
		case Texts::TextType::InsufficientMagickaForInstantCast:
			return "InsufficientMagickaForInstantCast";
		case Texts::TextType::KeymapReservedByFramework:
			return "KeymapReservedByFramework";
		case Texts::TextType::KeymapAlreadyBoundTo:
			return "KeymapAlreadyBoundTo";
		case Texts::TextType::KeymapUnbindButton:
			return "KeymapUnbindButton";
		case Texts::TextType::KeymapWrongDevice:
			return "KeymapWrongDevice";
		case Texts::TextType::SettingsLiveInFramework:
			return "SettingsLiveInFramework";
		case Texts::TextType::PresetsHeader:
			return "PresetsHeader";
		case Texts::TextType::PresetsHelp:
			return "PresetsHelp";
		case Texts::TextType::PresetLabel:
			return "PresetLabel";
		case Texts::TextType::PresetNone:
			return "PresetNone";
		case Texts::TextType::PresetNameLabel:
			return "PresetNameLabel";
		case Texts::TextType::PresetSaveAs:
			return "PresetSaveAs";
		case Texts::TextType::PresetRename:
			return "PresetRename";
		case Texts::TextType::PresetDelete:
			return "PresetDelete";
		case Texts::TextType::PresetResetDefaults:
			return "PresetResetDefaults";
		case Texts::TextType::PresetReloadIni:
			return "PresetReloadIni";
		case Texts::TextType::PresetSaved:
			return "PresetSaved";
		case Texts::TextType::PresetLoaded:
			return "PresetLoaded";
		case Texts::TextType::PresetRenamed:
			return "PresetRenamed";
		case Texts::TextType::PresetDeleted:
			return "PresetDeleted";
		case Texts::TextType::PresetNoneSelected:
			return "PresetNoneSelected";
		case Texts::TextType::PresetDefaultsRestored:
			return "PresetDefaultsRestored";
		case Texts::TextType::PresetReloaded:
			return "PresetReloaded";
		case Texts::TextType::PresetInvalidName:
			return "PresetInvalidName";
		case Texts::TextType::PresetNotFound:
			return "PresetNotFound";
		case Texts::TextType::PresetAlreadyExists:
			return "PresetAlreadyExists";
		case Texts::TextType::AdvancedSettingsHidden:
			return "AdvancedSettingsHidden";
		case Texts::TextType::EditHintActionToggleWheel:
			return "EditHintActionToggleWheel";
		case Texts::TextType::EditHintActionNextItem:
			return "EditHintActionNextItem";
		case Texts::TextType::EditHintActionPreviousItem:
			return "EditHintActionPreviousItem";
		case Texts::TextType::EditHintActionRotateWheel:
			return "EditHintActionRotateWheel";
		case Texts::TextType::EditHintActionToggleHints:
			return "EditHintActionToggleHints";
		case Texts::TextType::WheelCountLabel:
			return "WheelCountLabel";
		case Texts::TextType::WheelCountHelp:
			return "WheelCountHelp";
		case Texts::TextType::WheelCountBlockedByFilledWheel:
			return "WheelCountBlockedByFilledWheel";
		default:
			return "";
		}
	}
}

void Texts::LoadTranslations()
{
	// Base texts from INI (existing behavior)
	CSimpleIniA ini;
	ini.SetUnicode();
#define TEXTS_PATH "Data\\SKSE\\Plugins\\wheeler\\Texts.ini"
	ini.LoadFile(TEXTS_PATH);
	try {
		for (auto& [textType, text] : _textData) {
			const char* key = GetKeyForTextType(textType);
			if (key && *key) {
				loadTextFromIni(ini, key, text);
			}
		}
	}
	catch (std::exception e) {
		ERROR("Error loading from Texts.ini: {}", e.what());
	}

	// Optional overrides from a simple UTF-8 translation file:
	// Data\SKSE\Plugins\wheeler\translations.txt
	const std::string path = "Data\\SKSE\\Plugins\\wheeler\\translations.txt";
	std::ifstream file(path);
	if (!file.is_open()) {
		INFO("Custom text translation file not found: {}", path);
		return;
	}

	INFO("Loading Wheeler text translations from {}", path);

	std::unordered_map<std::string, std::string> overrides;
	std::unordered_map<std::string, int> keyFirstLineNum;  // Track first occurrence line for duplicate warnings
	std::string line;
	int lineNum = 0;
	int entryCount = 0;
	while (std::getline(file, line)) {
		++lineNum;
		if (line.empty()) {
			continue;
		}
		// handle UTF-8 BOM if present on first line
		if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF &&
			static_cast<unsigned char>(line[1]) == 0xBB &&
			static_cast<unsigned char>(line[2]) == 0xBF) {
			line.erase(0, 3);
		}
		if (line.empty() || line[0] == '#' || line[0] == ';') {
			continue;
		}

		auto pos = line.find_first_of("=\t");
		if (pos == std::string::npos) {
			continue;
		}

		std::string key = trim(line.substr(0, pos));
		std::string value = trim(line.substr(pos + 1));
		if (key.empty()) {
			continue;
		}

		// Handle duplicates with logging
		if (auto it = overrides.find(key); it != overrides.end()) {
			// Duplicate key detected
			int firstLine = keyFirstLineNum[key];
			if (!value.empty()) {
				// New value is non-empty: override
				WARN("translations.txt: Duplicate key '{}' at line {} (first at line {}). Non-empty value wins: '{}'",
					key, lineNum, firstLine, value);
				it->second = value;
			} else {
				// New value is empty: keep existing non-empty value
				WARN("translations.txt: Duplicate key '{}' at line {} (first at line {}). Keeping existing non-empty value.",
					key, lineNum, firstLine);
			}
		} else {
			// First occurrence of this key
			if (!value.empty()) {
				overrides[key] = value;
				keyFirstLineNum[key] = lineNum;
				++entryCount;
			}
		}
	}

	for (auto& [textType, text] : _textData) {
		const char* id = GetKeyForTextType(textType);
		if (!id || *id == '\0') {
			continue;
		}
		if (auto it = overrides.find(id); it != overrides.end()) {
			text = it->second;
		}
	}

	INFO("Loaded {} translation entries from {}", entryCount, path);
	_englishData = _textData;
	LoadLanguageFile();
}

namespace
{
	std::string LowerAscii(std::string a_s)
	{
		std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_s;
	}

	std::string GameLanguage()
	{
		std::string lang = "english";
		if (auto* ini = RE::INISettingCollection::GetSingleton()) {
			if (auto* setting = ini->GetSetting("sLanguage:General"); setting && setting->GetString() && setting->GetString()[0]) {
				lang = setting->GetString();
			}
		}
		return LowerAscii(lang);
	}

	std::string Utf16ToUtf8(const std::wstring& a_w)
	{
		if (a_w.empty()) { return {}; }
		const int n = WideCharToMultiByte(CP_UTF8, 0, a_w.data(), static_cast<int>(a_w.size()), nullptr, 0, nullptr, nullptr);
		std::string out(static_cast<std::size_t>(n), '\0');
		WideCharToMultiByte(CP_UTF8, 0, a_w.data(), static_cast<int>(a_w.size()), out.data(), n, nullptr, nullptr);
		return out;
	}

	// The SkyUI/SKSE translation file: UTF-16LE with BOM, "$Key<TAB>text" per line; "\n" in a text is a
	// line break. Returns -1 when the file is missing, -2 when it is not UTF-16LE.
	int ReadTranslationFile(const std::string& a_path, std::unordered_map<std::string, std::string>& a_out)
	{
		std::ifstream in(a_path, std::ios::binary);
		if (!in) { return -1; }
		std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		if (bytes.size() < 2 || static_cast<unsigned char>(bytes[0]) != 0xFF || static_cast<unsigned char>(bytes[1]) != 0xFE) { return -2; }
		std::wstring text(reinterpret_cast<const wchar_t*>(bytes.data() + 2), (bytes.size() - 2) / 2);
		int added = 0;
		std::size_t pos = 0;
		while (pos < text.size()) {
			auto eol = text.find(L'\n', pos);
			if (eol == std::wstring::npos) { eol = text.size(); }
			std::wstring line = text.substr(pos, eol - pos);
			pos = eol + 1;
			if (!line.empty() && line.back() == L'\r') { line.pop_back(); }
			if (line.empty() || line[0] != L'$') { continue; }
			const auto tab = line.find(L'\t');
			if (tab == std::wstring::npos) { continue; }
			std::string key = Utf16ToUtf8(line.substr(1, tab - 1));
			std::wstring value = line.substr(tab + 1);
			for (std::size_t i = 0; i + 1 < value.size(); ++i) {
				if (value[i] == L'\\' && value[i + 1] == L'n') { value.replace(i, 2, L"\n"); }
			}
			if (value.empty()) { continue; }
			a_out[key] = Utf16ToUtf8(value);
			++added;
		}
		return added;
	}
}

void Texts::LoadLanguageFile(const std::string& a_forceLanguage)
{
	// Whatever language ends up in force, the atlas must hold its glyphs (1.2.7).
	struct RebuildOnExit { ~RebuildOnExit() { RenderManager::RequestFontRebuild(); } } rebuild;
	const std::string lang = a_forceLanguage.empty() ? GameLanguage() : LowerAscii(a_forceLanguage);
	_language = lang;
	_languageEntries = 0;
	_languageFile.clear();
	if (!_englishData.empty()) {
		_textData = _englishData;   // start from English so a switch never keeps the previous language's leftovers
	}
	if (lang == "english") {
		INFO("Texts: language 'english' - the shipped English strings are in force");
		return;
	}
	const std::string path = "Data\\Interface\\Translations\\Wheeler_" + lang + ".txt";
	std::unordered_map<std::string, std::string> texts;
	const int n = ReadTranslationFile(path, texts);
	if (n == -1) {
		WARN("Texts: no translation file for '{}' ({}); English stays in force", lang, path);
		return;
	}
	if (n == -2) {
		WARN("Texts: '{}' is not UTF-16LE with a BOM; English stays in force", path);
		return;
	}
	int applied = 0;
	for (auto& [textType, text] : _textData) {
		const char* id = GetKeyForTextType(textType);
		if (!id || *id == '\0') { continue; }
		if (auto it = texts.find(id); it != texts.end()) {
			text = it->second;
			++applied;
		}
	}
	_languageFile = path;
	_languageEntries = applied;
	INFO("Texts: language '{}' - {} of {} keys applied from {}", lang, applied, n, path);
}

const std::string& Texts::Language() { return _language; }

std::string Texts::AllText()
{
	std::string out;
	for (const auto& [textType, text] : _textData) {
		out += text;
		out += '\n';
	}
	return out;
}
const std::string& Texts::LanguageFile() { return _languageFile; }
int Texts::LanguageEntries() { return _languageEntries; }

const char* Texts::GetText(TextType a_textType)
{
	return _textData[a_textType].data();
}
