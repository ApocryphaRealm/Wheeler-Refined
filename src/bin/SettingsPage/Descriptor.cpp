#include "Descriptor.h"

#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace SettingsPage
{
	namespace
	{
		// Parsing never descends into "control" - the {id, type, value} predicates in
		// control.requirements carry a "type" key and would otherwise be counted as controls.
		constexpr const char* kEntriesKey = "entries";
		constexpr const char* kControlKey = "control";

		std::optional<double> AsNumber(const nlohmann::json& a_value)
		{
			if (a_value.is_number()) {
				return a_value.get<double>();
			}
			if (a_value.is_boolean()) {
				return a_value.get<bool>() ? 1.0 : 0.0;
			}
			return std::nullopt;
		}

		std::string AsString(const nlohmann::json& a_parent, const char* a_key)
		{
			if (a_parent.contains(a_key) && a_parent[a_key].is_string()) {
				return a_parent[a_key].get<std::string>();
			}
			return {};
		}

		void ParseText(const nlohmann::json& a_node, Entry& a_entry)
		{
			if (!a_node.contains("text") || !a_node["text"].is_object()) {
				return;
			}
			const auto& text = a_node["text"];
			a_entry.name = AsString(text, "name");
			a_entry.desc = AsString(text, "desc");
		}

		void ParseIniTarget(const nlohmann::json& a_node, Entry& a_entry)
		{
			if (!a_node.contains("ini") || !a_node["ini"].is_object()) {
				return;
			}
			const auto& ini = a_node["ini"];
			a_entry.iniSection = AsString(ini, "section");
			a_entry.iniKey = AsString(ini, "id");
		}

		void ParseSliderStyle(const nlohmann::json& a_node, Entry& a_entry)
		{
			if (!a_node.contains("style") || !a_node["style"].is_object()) {
				return;
			}
			const auto& style = a_node["style"];
			SliderStyle out{};
			if (const auto min = style.contains("min") ? AsNumber(style["min"]) : std::nullopt) {
				out.min = *min;
			}
			if (const auto max = style.contains("max") ? AsNumber(style["max"]) : std::nullopt) {
				out.max = *max;
			}
			if (const auto step = style.contains("step") ? AsNumber(style["step"]) : std::nullopt) {
				out.step = *step;
			}
			const auto format = AsString(style, "format");
			if (!format.empty()) {
				out.format = format;
			}
			a_entry.slider = out;
		}

		void ParseDefault(const nlohmann::json& a_node, Entry& a_entry)
		{
			if (!a_node.contains("default")) {
				return;
			}
			const auto& value = a_node["default"];

			// Colour defaults are {r,g,b,a} floats in 0..1, not a packed integer.
			if (value.is_object()) {
				ColorValue colour{};
				bool any = false;
				const auto component = [&](const char* a_key, float& a_out) {
					if (value.contains(a_key)) {
						if (const auto number = AsNumber(value[a_key])) {
							a_out = static_cast<float>(*number);
							any = true;
						}
					}
				};
				component("r", colour.r);
				component("g", colour.g);
				component("b", colour.b);
				component("a", colour.a);
				if (any) {
					a_entry.defaultColor = colour;
				}
				return;
			}

			if (value.is_boolean()) {
				a_entry.defaultBool = value.get<bool>();
				return;
			}
			if (value.is_number()) {
				a_entry.defaultNumber = value.get<double>();
				return;
			}
			if (value.is_string()) {
				a_entry.defaultString = value.get<std::string>();
			}
		}

		void ParseOptions(const nlohmann::json& a_node, Entry& a_entry)
		{
			if (!a_node.contains("options") || !a_node["options"].is_array()) {
				return;
			}
			for (const auto& option : a_node["options"]) {
				if (option.is_string()) {
					a_entry.options.push_back(option.get<std::string>());
				}
			}
		}

		void ParseControl(const nlohmann::json& a_node, Entry& a_entry)
		{
			if (!a_node.contains(kControlKey) || !a_node[kControlKey].is_object()) {
				return;
			}
			const auto& control = a_node[kControlKey];

			// Only when the top-level id did not already supply one - see ParseEntry. Overwriting
			// unconditionally would blank a button's action id, since buttons have control without
			// a control.id.
			if (a_entry.id.empty()) {
				a_entry.id = AsString(control, "id");
			}

			const auto failAction = AsString(control, "failAction");
			a_entry.failAction = (failAction == "hide") ? FailAction::Hide : FailAction::Disable;

			if (!control.contains("requirements") || !control["requirements"].is_array()) {
				return;
			}
			for (const auto& node : control["requirements"]) {
				if (!node.is_object()) {
					continue;
				}
				Requirement requirement{};
				requirement.id = AsString(node, "id");
				requirement.type = EntryTypeFromString(AsString(node, "type"));
				if (node.contains("value")) {
					const auto& value = node["value"];
					if (value.is_boolean()) {
						requirement.boolValue = value.get<bool>();
					} else if (value.is_number()) {
						requirement.numberValue = value.get<double>();
					} else if (value.is_string()) {
						requirement.stringValue = value.get<std::string>();
					}
				}
				if (!requirement.id.empty()) {
					a_entry.requirements.push_back(std::move(requirement));
				}
			}
		}

		Entry ParseEntry(const nlohmann::json& a_node)
		{
			Entry entry{};
			entry.type = EntryTypeFromString(AsString(a_node, "type"));

			// An entry's id can live in EITHER place, and buttons use the one ParseControl does not
			// look at. All five button entries carry their action id at the TOP level
			// ("wheeler_reset_all_wheels" and friends) with control.id absent - so reading only
			// control.id left Entry::id empty for every button, and a dispatch on it would have
			// rendered and silently done nothing.
			//
			// Read the top-level one first; ParseControl still fills it from control.id when that
			// is where it lives (84 entries). The 286 other top-level ids belong to the requirement
			// predicates inside control.requirements, which never reach here - they are parsed
			// separately as Requirement.id.
			entry.id = AsString(a_node, "id");

			ParseText(a_node, entry);
			ParseIniTarget(a_node, entry);
			ParseControl(a_node, entry);
			ParseDefault(a_node, entry);
			ParseSliderStyle(a_node, entry);
			ParseOptions(a_node, entry);

			if (a_node.contains(kEntriesKey) && a_node[kEntriesKey].is_array()) {
				for (const auto& child : a_node[kEntriesKey]) {
					if (child.is_object()) {
						entry.entries.push_back(ParseEntry(child));
					}
				}
			}

			return entry;
		}

		void IndexEntries(const std::vector<Entry>& a_entries, std::map<std::string, const Entry*>& a_out)
		{
			for (const auto& entry : a_entries) {
				if (!entry.id.empty()) {
					a_out.emplace(entry.id, &entry);
				}
				IndexEntries(entry.entries, a_out);
			}
		}

		void MeasureEntries(const std::vector<Entry>& a_entries, Census& a_census, int a_depth)
		{
			for (const auto& entry : a_entries) {
				a_census.nodes += 1;
				a_census.byType[entry.type] += 1;
				a_census.requirements += static_cast<int>(entry.requirements.size());
				a_census.maxDepth = (std::max)(a_census.maxDepth, a_depth);

				if (entry.type == EntryType::Group) {
					a_census.groups += 1;
				} else if (entry.type == EntryType::Text) {
					a_census.labels += 1;
				} else if (IsControl(entry.type)) {
					a_census.controls += 1;
				}

				MeasureEntries(entry.entries, a_census, a_depth + 1);
			}
		}

		int CountNodes(const std::vector<Entry>& a_entries)
		{
			int total = 0;
			for (const auto& entry : a_entries) {
				total += 1 + CountNodes(entry.entries);
			}
			return total;
		}
	}

	const char* EntryTypeName(EntryType a_type)
	{
		switch (a_type) {
		case EntryType::Group:
			return "group";
		case EntryType::Text:
			return "text";
		case EntryType::Checkbox:
			return "checkbox";
		case EntryType::Slider:
			return "slider";
		case EntryType::Keymap:
			return "keymap";
		case EntryType::Color:
			return "color";
		case EntryType::Dropdown:
			return "dropdown";
		case EntryType::Textbox:
			return "textbox";
		case EntryType::Button:
			return "button";
		default:
			return "unknown";
		}
	}

	EntryType EntryTypeFromString(std::string_view a_name)
	{
		if (a_name == "group") {
			return EntryType::Group;
		}
		if (a_name == "text") {
			return EntryType::Text;
		}
		if (a_name == "checkbox") {
			return EntryType::Checkbox;
		}
		if (a_name == "slider") {
			return EntryType::Slider;
		}
		if (a_name == "keymap") {
			return EntryType::Keymap;
		}
		if (a_name == "color" || a_name == "colour") {
			return EntryType::Color;
		}
		if (a_name == "dropdown") {
			return EntryType::Dropdown;
		}
		if (a_name == "textbox") {
			return EntryType::Textbox;
		}
		if (a_name == "button") {
			return EntryType::Button;
		}
		return EntryType::Unknown;
	}

	bool IsControl(EntryType a_type)
	{
		switch (a_type) {
		case EntryType::Checkbox:
		case EntryType::Slider:
		case EntryType::Keymap:
		case EntryType::Color:
		case EntryType::Dropdown:
		case EntryType::Textbox:
		case EntryType::Button:
			return true;
		default:
			return false;
		}
	}

	std::string NormalizeIniPath(std::string_view a_path)
	{
		std::string out;
		out.reserve(a_path.size());

		bool lastWasSeparator = false;
		for (const char character : a_path) {
			const char normalized = (character == '/') ? '\\' : character;
			if (normalized == '\\') {
				if (lastWasSeparator) {
					continue;  // collapse the doubled separators in Wheel Behavior / Wheeler I4
				}
				lastWasSeparator = true;
			} else {
				lastWasSeparator = false;
			}
			out.push_back(normalized);
		}

		const auto first = out.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			return {};
		}
		const auto last = out.find_last_not_of(" \t\r\n");
		return out.substr(first, last - first + 1);
	}

	Catalog& Catalog::GetSingleton()
	{
		static Catalog singleton;
		return singleton;
	}

	bool Catalog::LoadAll(const char* a_directory)
	{
		_panels.clear();
		_byId.clear();
		_loaded = false;

		std::error_code ec;
		const std::filesystem::path root(a_directory);
		if (!std::filesystem::exists(root, ec) || ec || !std::filesystem::is_directory(root, ec)) {
			WARN("[SettingsPage] Descriptor directory not found: {}", a_directory);
			return false;
		}

		// Sorted so panel order is the same on every machine and every launch.
		std::vector<std::filesystem::path> files;
		for (const auto& item : std::filesystem::directory_iterator(root, ec)) {
			if (ec) {
				break;
			}
			const auto& path = item.path();
			if (!item.is_regular_file()) {
				continue;
			}
			auto extension = path.extension().string();
			std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
				return static_cast<char>(std::tolower(c));
			});
			if (extension == ".json") {
				files.push_back(path);
			}
		}
		// 1.0.7: panels open in the order a player needs them, not alphabetically - alphabetical put
		// "Action Hotkeys Bridge Layout" (a legacy helper) on the first tab. Unknown files keep their
		// alphabetical place after the known ones.
		const auto rank = [](const std::filesystem::path& a_path) {
			static const char* kOrder[] = { "wheeler controls", "wheel behavior", "ammo wheel", "wheeler styles",
				"wheeler i4", "action hotkeys bridge", "ostim integration", "action hotkeys bridge layout" };
			std::string stem = a_path.stem().string();
			std::transform(stem.begin(), stem.end(), stem.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			for (std::size_t i = 0; i < std::size(kOrder); ++i) {
				if (stem == kOrder[i]) {
					return static_cast<int>(i);
				}
			}
			return 100;
		};
		std::sort(files.begin(), files.end());
		std::stable_sort(files.begin(), files.end(), [&](const auto& a, const auto& b) { return rank(a) < rank(b); });

		for (const auto& path : files) {
			std::ifstream input(path);
			if (!input.good()) {
				WARN("[SettingsPage] Could not open descriptor '{}'", path.filename().string());
				continue;
			}

			nlohmann::json json;
			try {
				input >> json;
			} catch (const std::exception& e) {
				// One malformed descriptor - possibly another mod's - must not cost the player the
				// whole settings page, so it is reported and skipped rather than fatal.
				//
				// (void)e keeps the standalone proof harness warning-free: it stubs ERROR(...) to a
				// no-op, which would otherwise leave `e` unreferenced and emit C4101 on every run.
				(void)e;
				ERROR("[SettingsPage] Descriptor '{}' is not valid JSON: {}", path.filename().string(), e.what());
				continue;
			}

			if (!json.is_object() || !json.contains("data") || !json["data"].is_array()) {
				WARN("[SettingsPage] Descriptor '{}' has no 'data' array, skipped", path.filename().string());
				continue;
			}

			Panel panel{};
			panel.sourceFile = path.filename().string();
			panel.name = AsString(json, "name");
			panel.rawIniPath = AsString(json, "ini");
			panel.iniPath = NormalizeIniPath(panel.rawIniPath);
			if (panel.name.empty()) {
				panel.name = path.stem().string();
			}

			for (const auto& node : json["data"]) {
				if (node.is_object()) {
					panel.entries.push_back(ParseEntry(node));
				}
			}

			if (panel.iniPath != panel.rawIniPath) {
				INFO("[SettingsPage] Normalised INI path for '{}': '{}' -> '{}'",
					panel.name, panel.rawIniPath, panel.iniPath);
			}

			_panels.push_back(std::move(panel));
		}

		_byId.resize(_panels.size());
		for (std::size_t i = 0; i < _panels.size(); ++i) {
			IndexEntries(_panels[i].entries, _byId[i]);
		}

		_loaded = !_panels.empty();
		return _loaded;
	}

	const Entry* Catalog::FindById(const Panel& a_panel, std::string_view a_id) const
	{
		for (std::size_t i = 0; i < _panels.size(); ++i) {
			if (&_panels[i] != &a_panel) {
				continue;
			}
			const auto found = _byId[i].find(std::string(a_id));
			return (found == _byId[i].end()) ? nullptr : found->second;
		}
		return nullptr;
	}

	Census Catalog::Measure() const
	{
		Census census{};
		census.panels = static_cast<int>(_panels.size());
		for (const auto& panel : _panels) {
			MeasureEntries(panel.entries, census, 0);
			census.nodesByFile[panel.sourceFile] = CountNodes(panel.entries);
		}
		return census;
	}

	void Catalog::LogCensus() const
	{
		const auto census = Measure();

		INFO("[SettingsPage] Descriptor census: {} panels, {} nodes, {} controls, {} groups, {} labels, max depth {}, {} requirement predicates",
			census.panels, census.nodes, census.controls, census.groups, census.labels,
			census.maxDepth, census.requirements);

		for (const auto& [type, count] : census.byType) {
			INFO("[SettingsPage]   type {:<9} {}", EntryTypeName(type), count);
		}

		for (const auto& panel : _panels) {
			const auto found = census.nodesByFile.find(panel.sourceFile);
			const int nodes = (found == census.nodesByFile.end()) ? 0 : found->second;
			INFO("[SettingsPage]   {:<36} -> {:<48} nodes={}",
				panel.sourceFile, panel.iniPath, nodes);
		}
	}
}
