#include "ValueStore.h"

#include <SimpleIni.h>

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>

namespace SettingsPage
{
	namespace
	{
		// The defaults/factory partner for each live target, taken from Config.cpp's *_PATH macros
		// rather than built by string munging. Two reasons it must be a table: the suffix is
		// ".defaults" for six files but ".factory" for wheelBehavior, and the descriptors spell the
		// folder both "wheeler" and "Wheeler". ActionHotkeysBridge.layout.ini deliberately has no
		// partner - the game writes that file, nothing ships it.
		struct Pairing
		{
			const char* liveBasename;
			const char* defaultsBasename;  // nullptr = nothing ships
		};

		constexpr Pairing kPairings[] = {
			{ "styles.ini", "Styles.defaults.ini" },
			{ "i4.ini", "I4.defaults.ini" },
			{ "wheelbehavior.ini", "wheelBehavior.factory.ini" },
			{ "controls.ini", "Controls.defaults.ini" },
			{ "actionhotkeysbridge.ini", "ActionHotkeysBridge.defaults.ini" },
			{ "ostimintegration.ini", "OStimIntegration.defaults.ini" },
			{ "ammowheel.ini", "AmmoWheel.defaults.ini" },
			{ "actionhotkeysbridge.layout.ini", nullptr },
		};

		std::string ToLower(std::string a_text)
		{
			std::transform(a_text.begin(), a_text.end(), a_text.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			return a_text;
		}

		std::string BaseName(const std::string& a_path)
		{
			const auto slash = a_path.find_last_of("\\/");
			return (slash == std::string::npos) ? a_path : a_path.substr(slash + 1);
		}

		const char* DefaultsFor(const std::string& a_livePath)
		{
			const std::string base = ToLower(BaseName(a_livePath));
			for (const auto& pairing : kPairings) {
				if (base == pairing.liveBasename) {
					return pairing.defaultsBasename;
				}
			}
			return nullptr;
		}

		std::string JoinPath(const std::string& a_directory, const std::string& a_leaf)
		{
			if (a_directory.empty()) {
				return a_leaf;
			}
			const char last = a_directory.back();
			return (last == '\\' || last == '/') ? (a_directory + a_leaf) : (a_directory + "\\" + a_leaf);
		}
	}

	struct ValueStore::PanelFiles
	{
		std::string userPath;
		std::string defaultsPath;
		CSimpleIniA user;
		CSimpleIniA defaults;
		bool userLoaded{ false };
		bool defaultsLoaded{ false };
	};

	const char* ValueSourceName(ValueSource a_source)
	{
		switch (a_source) {
		case ValueSource::DefaultsFile:
			return "defaults";
		case ValueSource::UserFile:
			return "user";
		default:
			return "none";
		}
	}

	ColorValue ColorFromPacked(std::uint32_t a_packed)
	{
		ColorValue colour{};
		colour.r = static_cast<float>(a_packed & 0xFFu) / 255.0f;
		colour.g = static_cast<float>((a_packed >> 8) & 0xFFu) / 255.0f;
		colour.b = static_cast<float>((a_packed >> 16) & 0xFFu) / 255.0f;
		colour.a = static_cast<float>((a_packed >> 24) & 0xFFu) / 255.0f;
		return colour;
	}

	std::uint32_t ColorToPacked(const ColorValue& a_color)
	{
		const auto channel = [](float value) -> std::uint32_t {
			const float clamped = (std::max)(0.0f, (std::min)(1.0f, value));
			return static_cast<std::uint32_t>(clamped * 255.0f + 0.5f);
		};
		return (channel(a_color.a) << 24) | (channel(a_color.b) << 16) |
		       (channel(a_color.g) << 8) | channel(a_color.r);
	}

	bool ResolvedValue::AsBool(bool a_fallback) const
	{
		if (!Found() || raw.empty()) {
			return a_fallback;
		}
		// CSimpleIni's rule, which is what Wheeler actually gets: it looks at the FIRST character
		// only. t/y/1 true; f/n/0 false; o decided by the second character (on/off); anything else
		// keeps the default.
		switch (raw[0]) {
		case 't': case 'T':
		case 'y': case 'Y':
		case '1':
			return true;
		case 'f': case 'F':
		case 'n': case 'N':
		case '0':
			return false;
		case 'o': case 'O':
			if (raw.size() > 1 && (raw[1] == 'n' || raw[1] == 'N')) {
				return true;
			}
			if (raw.size() > 1 && (raw[1] == 'f' || raw[1] == 'F')) {
				return false;
			}
			break;
		default:
			break;
		}
		return a_fallback;
	}

	double ResolvedValue::AsNumber(double a_fallback) const
	{
		if (!Found() || raw.empty()) {
			return a_fallback;
		}
		// std::stof parses a leading number and ignores trailing text, which is what GetFloatValue
		// does. strtod is used instead so an out-of-range value cannot throw - GetFloatValue catches
		// only std::invalid_argument, leaving std::out_of_range to escape, and that fragility is not
		// worth reproducing.
		const char* begin = raw.c_str();
		char* end = nullptr;
		errno = 0;
		const double parsed = std::strtod(begin, &end);
		if (end == begin) {
			return a_fallback;
		}
		if (errno == ERANGE) {
			return a_fallback;
		}
		return parsed;
	}

	std::uint32_t ResolvedValue::AsUInt32(std::uint32_t a_fallback) const
	{
		if (!Found() || raw.empty()) {
			return a_fallback;
		}

		const char* cursor = raw.c_str();
		while (*cursor && std::isspace(static_cast<unsigned char>(*cursor))) {
			++cursor;
		}
		if (!*cursor) {
			return a_fallback;
		}

		char* end = nullptr;
		errno = 0;
		// Base 0: auto-detects decimal and 0x, matching GetUInt32Value. Colours are written decimal
		// in the shipped files but presets/N/Styles.ini uses 0xC8A08C6E, so both really occur.
		unsigned long long parsed = std::strtoull(cursor, &end, 0);
		if (end == cursor) {
			return a_fallback;
		}

		// A ".000000" tail is ACCEPTED - dMenu serialises its float sliders that way and
		// GetUInt32Value tolerates it deliberately. Anything else trailing is a parse failure.
		if (end && *end == '.') {
			const char* fraction = end + 1;
			while (*fraction && std::isdigit(static_cast<unsigned char>(*fraction))) {
				++fraction;
			}
			while (*fraction && std::isspace(static_cast<unsigned char>(*fraction))) {
				++fraction;
			}
			if (*fraction != '\0') {
				return a_fallback;
			}
		} else if (end) {
			const char* tail = end;
			while (*tail && std::isspace(static_cast<unsigned char>(*tail))) {
				++tail;
			}
			if (*tail != '\0') {
				return a_fallback;
			}
		}

		// Clamp rather than wrap, as GetUInt32Value does. 4294967295 is a real shipped value sitting
		// exactly at this ceiling.
		if (errno == ERANGE || parsed > (std::numeric_limits<std::uint32_t>::max)()) {
			parsed = (std::numeric_limits<std::uint32_t>::max)();
		}
		return static_cast<std::uint32_t>(parsed);
	}

	ColorValue ResolvedValue::AsColor(ColorValue a_fallback) const
	{
		if (!Found()) {
			return a_fallback;
		}
		const std::uint32_t packed = AsUInt32(ColorToPacked(a_fallback));
		return ColorFromPacked(packed);
	}

	std::string ResolvedValue::AsString(const std::string& a_fallback) const
	{
		return Found() ? raw : a_fallback;
	}

	ValueStore& ValueStore::GetSingleton()
	{
		static ValueStore singleton;
		return singleton;
	}

	bool ValueStore::LoadAll(const Catalog& a_catalog, const char* a_iniDirectory)
	{
		_panels.clear();
		_loaded = false;

		const std::string directory = a_iniDirectory ? a_iniDirectory : kDefaultIniDirectory;

		for (const auto& panel : a_catalog.Panels()) {
			auto files = std::make_shared<PanelFiles>();

			// BOTH layers resolve against the same directory. Taking the user path straight from
			// the descriptor while the defaults came from a_iniDirectory would have the store
			// reading its two layers out of two different places - which works by accident when the
			// game runs from the Skyrim root, and silently finds nothing anywhere else (the proof
			// harness runs from the repo root, and would have reported every key unresolved).
			// Only the basename is taken from the descriptor; the case flip between "wheeler" and
			// "Wheeler" in those paths is another reason not to trust the directory part.
			files->userPath = JoinPath(directory, BaseName(panel.iniPath));
			files->user.SetUnicode();
			files->userLoaded = files->user.LoadFile(files->userPath.c_str()) >= 0;

			if (const char* defaultsName = DefaultsFor(panel.iniPath)) {
				files->defaultsPath = JoinPath(directory, defaultsName);
				files->defaults.SetUnicode();
				files->defaultsLoaded = files->defaults.LoadFile(files->defaultsPath.c_str()) >= 0;
			}

			INFO("[SettingsPage] {}: defaults={} user={} ({})",
				panel.name,
				files->defaultsLoaded ? "yes" : "no",
				files->userLoaded ? "yes" : "no",
				files->defaultsPath.empty() ? "none ships" : files->defaultsPath);

			_panels.emplace(panel.sourceFile, std::move(files));
		}

		_loaded = !_panels.empty();
		return _loaded;
	}

	ResolvedValue ValueStore::Get(const Panel& a_panel, const std::string& a_section, const std::string& a_key) const
	{
		ResolvedValue resolved{};
		if (a_section.empty() || a_key.empty()) {
			return resolved;
		}

		const auto found = _panels.find(a_panel.sourceFile);
		if (found == _panels.end() || !found->second) {
			return resolved;
		}
		const PanelFiles& files = *found->second;

		// User first: it overrides the defaults key by key, exactly as MergeIniInto does.
		if (files.userLoaded) {
			if (const char* value = files.user.GetValue(a_section.c_str(), a_key.c_str(), nullptr)) {
				resolved.source = ValueSource::UserFile;
				resolved.raw = value;
				return resolved;
			}
		}
		if (files.defaultsLoaded) {
			if (const char* value = files.defaults.GetValue(a_section.c_str(), a_key.c_str(), nullptr)) {
				resolved.source = ValueSource::DefaultsFile;
				resolved.raw = value;
				return resolved;
			}
		}
		return resolved;
	}

	ResolvedValue ValueStore::Get(const Panel& a_panel, const Entry& a_entry) const
	{
		return Get(a_panel, a_entry.iniSection, a_entry.iniKey);
	}

	bool ValueStore::Set(const Panel& a_panel, const Entry& a_entry, const std::string& a_rawValue)
	{
		if (!a_entry.HasIniTarget()) {
			WARN("[SettingsPage] Refusing to write '{}': the descriptor gives it no ini target", a_entry.name);
			return false;
		}

		const auto found = _panels.find(a_panel.sourceFile);
		if (found == _panels.end() || !found->second) {
			return false;
		}
		PanelFiles& files = *found->second;

		// Load-modify-save on the USER file. Loading first is the whole point: building a fresh
		// CSimpleIniA and saving would drop every other key in the file.
		CSimpleIniA ini;
		ini.SetUnicode();
		const bool existed = ini.LoadFile(files.userPath.c_str()) >= 0;
		if (!existed && files.defaultsLoaded) {
			// Mirror Config::EnsureUserIniBootstrapped: a missing user file is seeded from the
			// shipped defaults rather than created holding one lonely key.
			std::error_code ec;
			std::filesystem::create_directories(
				std::filesystem::path(files.userPath).parent_path(), ec);
			files.defaults.SaveFile(files.userPath.c_str());
			ini.Reset();
			ini.SetUnicode();
			ini.LoadFile(files.userPath.c_str());
		}

		ini.SetValue(a_entry.iniSection.c_str(), a_entry.iniKey.c_str(), a_rawValue.c_str());
		if (ini.SaveFile(files.userPath.c_str()) < 0) {
			ERROR("[SettingsPage] Failed to save '{}'", files.userPath);
			return false;
		}

		// Read back. Under MO2 other mods ship files at these same virtual paths, so the write can
		// land in a copy that a higher-priority mod masks - the save succeeds and the change still
		// does not take. Better to report that than to let it look like the page did nothing.
		files.user.Reset();
		files.user.SetUnicode();
		files.userLoaded = files.user.LoadFile(files.userPath.c_str()) >= 0;

		const ResolvedValue after = Get(a_panel, a_entry);
		if (!after.Found() || after.raw != a_rawValue) {
			WARN("[SettingsPage] Wrote [{}] {} = '{}' to '{}' but read back '{}' - another mod may be "
			     "masking this file in the load order",
				a_entry.iniSection, a_entry.iniKey, a_rawValue, files.userPath,
				after.Found() ? after.raw : std::string("<nothing>"));
			return false;
		}

		return true;
	}

	ValueStore::Stats ValueStore::Measure(const Catalog& a_catalog) const
	{
		Stats stats{};

		for (const auto& panel : a_catalog.Panels()) {
			int resolvedHere = 0;
			int fallbackHere = 0;

			// Buttons hold no value and carry no ini target, so they are not counted either way -
			// including them made an earlier measurement pessimistic by 5.
			const std::function<void(const std::vector<Entry>&)> walk =
				[&](const std::vector<Entry>& entries) {
					for (const auto& entry : entries) {
						if (IsControl(entry.type) && entry.type != EntryType::Button &&
						    entry.HasIniTarget()) {
							stats.controls += 1;
							const auto value = Get(panel, entry);
							if (value.Found()) {
								resolvedHere += 1;
								stats.resolved += 1;
								if (value.source == ValueSource::UserFile) {
									stats.fromUser += 1;
								}
							} else {
								fallbackHere += 1;
								stats.fallback += 1;
							}
						}
						walk(entry.entries);
					}
				};
			walk(panel.entries);

			stats.resolvedByPanel[panel.sourceFile] = resolvedHere;
			stats.fallbackByPanel[panel.sourceFile] = fallbackHere;
		}

		return stats;
	}

	void ValueStore::LogSummary(const Catalog& a_catalog) const
	{
		const auto stats = Measure(a_catalog);
		INFO("[SettingsPage] Values: {} controls, {} resolved ({} from user overrides), {} with no value in any file",
			stats.controls, stats.resolved, stats.fromUser, stats.fallback);

		for (const auto& panel : a_catalog.Panels()) {
			const auto resolved = stats.resolvedByPanel.find(panel.sourceFile);
			const auto fallback = stats.fallbackByPanel.find(panel.sourceFile);
			INFO("[SettingsPage]   {:<36} resolved={} fallback={}",
				panel.sourceFile,
				resolved == stats.resolvedByPanel.end() ? 0 : resolved->second,
				fallback == stats.fallbackByPanel.end() ? 0 : fallback->second);
		}
	}
}
