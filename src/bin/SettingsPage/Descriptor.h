#pragma once

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Reader for the dMenu descriptor files that already describe every Wheeler setting.
//
// The page is built from Data\SKSE\Plugins\dmenu\customSettings\*.json rather than from
// hand-written control lists: the descriptors already carry each setting's INI section and key,
// its type, default, range and help text, so Refined's own future settings appear here with no
// work from us, and dMenu keeps working for anyone who still uses it.
//
// This header is the parsed model only. Nothing here draws, and nothing here writes an INI.
namespace SettingsPage
{
	enum class EntryType
	{
		Unknown,
		Group,     // structural: carries children in `entries`
		Text,      // static label, not a setting
		Checkbox,
		Slider,
		Keymap,
		Color,
		Dropdown,
		Textbox,
		Button
	};

	const char* EntryTypeName(EntryType a_type);
	EntryType EntryTypeFromString(std::string_view a_name);

	// True for the types that actually represent a setting the player can change.
	// Group and Text are structure, not settings.
	bool IsControl(EntryType a_type);

	struct ColorValue
	{
		float r{ 0.0f };
		float g{ 0.0f };
		float b{ 0.0f };
		float a{ 1.0f };
	};

	struct SliderStyle
	{
		double min{ 0.0 };
		double max{ 0.0 };
		double step{ 0.0 };
		std::optional<std::string> format;  // printf-style, present on 13 of 486
	};

	// A predicate from `control.requirements`. Measured shape is always {id, type, value},
	// where `id` names ANOTHER entry's `control.id` and `value` is the state it must hold.
	// These are NOT controls - they only look like one because they carry a `type` key, which is
	// what inflated the original 638/334-checkbox census.
	struct Requirement
	{
		std::string id;
		EntryType type{ EntryType::Unknown };
		std::optional<bool> boolValue;
		std::optional<double> numberValue;
		std::optional<std::string> stringValue;
	};

	// What to do with an entry whose requirements are not met.
	// Measured across all eight descriptors: disable 931, hide 12.
	enum class FailAction
	{
		Disable,
		Hide
	};

	struct Entry
	{
		EntryType type{ EntryType::Unknown };

		std::string id;    // control.id - the handle requirements refer to. Often empty.
		std::string name;  // text.name
		std::string desc;  // text.desc

		std::string iniSection;  // ini.section
		std::string iniKey;      // ini.id

		std::optional<bool> defaultBool;
		std::optional<double> defaultNumber;
		std::optional<std::string> defaultString;
		std::optional<ColorValue> defaultColor;

		std::optional<SliderStyle> slider;
		std::vector<std::string> options;  // dropdown

		std::vector<Requirement> requirements;
		FailAction failAction{ FailAction::Disable };

		std::vector<Entry> entries;  // children, groups only

		bool HasIniTarget() const { return !iniSection.empty() && !iniKey.empty(); }
	};

	// One descriptor file: one settings panel, one target INI.
	struct Panel
	{
		std::string name;        // descriptor "name", e.g. "Wheeler Controls"
		std::string iniPath;     // descriptor "ini", NORMALISED - see NormalizeIniPath
		std::string rawIniPath;  // exactly as the descriptor spelled it, for diagnostics
		std::string sourceFile;  // file name only
		std::vector<Entry> entries;
	};

	// Two descriptors ("Wheel Behavior", "Wheeler I4") parse to DOUBLED backslashes where the other
	// six parse to single, and the folder case flips between "wheeler" and "Wheeler". Both are
	// harmless to Windows but fatal to string comparison, so every path goes through here:
	// forward slashes become backslashes, runs of backslashes collapse to one, and surrounding
	// whitespace is dropped. Case is left alone - the path is used, not just compared.
	std::string NormalizeIniPath(std::string_view a_path);

	struct Census
	{
		int panels{ 0 };
		int nodes{ 0 };     // every parsed node, groups and labels included
		int controls{ 0 };  // settings only
		int groups{ 0 };
		int labels{ 0 };
		int maxDepth{ 0 };
		int requirements{ 0 };
		std::map<EntryType, int> byType;
		std::map<std::string, int> nodesByFile;
	};

	// Loads every descriptor once and answers questions about them. Read-only after LoadAll.
	class Catalog
	{
	public:
		static Catalog& GetSingleton();

		// Parses every *.json under a_directory. Returns false only if the directory is missing or
		// no descriptor parsed; a single malformed file is logged and skipped, because one bad
		// descriptor from some other mod must not cost the player the whole settings page.
		bool LoadAll(const char* a_directory = kDefaultDirectory);

		const std::vector<Panel>& Panels() const { return _panels; }
		bool IsLoaded() const { return _loaded; }

		// Resolve a requirement's target within the panel that declared it.
		const Entry* FindById(const Panel& a_panel, std::string_view a_id) const;

		Census Measure() const;
		void LogCensus() const;

		static constexpr const char* kDefaultDirectory = "Data\\SKSE\\Plugins\\dmenu\\customSettings";

	private:
		Catalog() = default;

		std::vector<Panel> _panels;
		// id -> entry, per panel, built once after parsing. Safe because the tree is never mutated
		// afterwards; building it during parsing would dangle as the vectors grew.
		std::vector<std::map<std::string, const Entry*>> _byId;
		bool _loaded{ false };
	};
}
