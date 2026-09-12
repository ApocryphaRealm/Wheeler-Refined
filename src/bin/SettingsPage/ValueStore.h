#pragma once

#include "Descriptor.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

// Reads and writes the INI values behind the descriptor tree.
//
// The page must show what a setting IS, not what the descriptor says it defaults to - those
// disagree for 30 of 43 comparable Styles keys, sometimes wildly (CenterOffsetX: descriptor 0,
// compiled 450). So every displayed value comes from a file, and where no file has the key the
// store says so rather than inventing one.
//
// Resolution mirrors Config::LoadLayeredIni: the shipped defaults/factory file, then the user file
// over it KEY BY KEY. The two layers are kept as separate objects rather than merged, because the
// lookup order then yields the provenance for free - the page can say "this is your override"
// versus "this is the shipped default" without a second pass.
namespace SettingsPage
{
	enum class ValueSource
	{
		NotFound,      // neither layer has the key
		DefaultsFile,  // shipped X.defaults.ini / X.factory.ini
		UserFile       // the player's X.ini, which overrides
	};

	const char* ValueSourceName(ValueSource a_source);

	// One resolved setting. `raw` is exactly the text the file held, so a write can round-trip the
	// same notation it found.
	struct ResolvedValue
	{
		ValueSource source{ ValueSource::NotFound };
		std::string raw;

		bool Found() const { return source != ValueSource::NotFound; }

		// Parsing mirrors Wheeler's own readers exactly. Matching its behaviour matters more than
		// being independently "correct": if the page and the mod disagree about what a file says,
		// the player sees one thing and gets another.
		bool AsBool(bool a_fallback) const;
		double AsNumber(double a_fallback) const;
		std::uint32_t AsUInt32(std::uint32_t a_fallback) const;
		ColorValue AsColor(ColorValue a_fallback) const;
		std::string AsString(const std::string& a_fallback) const;
	};

	// Packed ABGR (0xAABBGGRR) <-> normalised RGBA. This is ImGui's own order with
	// IMGUI_USE_BGRA_PACKED_COLOR undefined, and it matches Config.cpp's unpackColor
	// (r = packed & 0xFF ... a = packed >> 24). The "0xAARRGGBB" comment on GetHexColor is stale.
	ColorValue ColorFromPacked(std::uint32_t a_packed);
	std::uint32_t ColorToPacked(const ColorValue& a_color);

	class ValueStore
	{
	public:
		static ValueStore& GetSingleton();

		// Opens both layers for every panel in the catalogue. Missing files are not an error:
		// on a fresh install no user INI exists at all (Config::EnsureUserIniBootstrapped creates
		// it on first run), and one descriptor ships no defaults file by design.
		bool LoadAll(const Catalog& a_catalog, const char* a_iniDirectory = kDefaultIniDirectory);

		ResolvedValue Get(const Panel& a_panel, const Entry& a_entry) const;
		ResolvedValue Get(const Panel& a_panel, const std::string& a_section, const std::string& a_key) const;

		// Writes one key to the USER file, load-modify-save. Never rebuilds the file from empty -
		// that is what WriteActionHotkeysBridgeLayoutConfig does, and copying it here would erase
		// every other setting in the file on the first slider a player moved.
		//
		// Then READS THE VALUE BACK. Under MO2 several mods ship files at these same virtual paths,
		// so a write can land in a file that a higher-priority mod masks: the save succeeds, the
		// reload reads the other copy, and the change silently vanishes. The read-back turns that
		// into something the page can report instead of a mystery.
		bool Set(const Panel& a_panel, const Entry& a_entry, const std::string& a_rawValue);

		struct Stats
		{
			int controls{ 0 };
			int resolved{ 0 };
			int fallback{ 0 };
			int fromUser{ 0 };
			std::map<std::string, int> resolvedByPanel;
			std::map<std::string, int> fallbackByPanel;
		};

		Stats Measure(const Catalog& a_catalog) const;
		void LogSummary(const Catalog& a_catalog) const;

		static constexpr const char* kDefaultIniDirectory = "Data\\SKSE\\Plugins\\wheeler";

	private:
		ValueStore() = default;

		struct PanelFiles;
		// Keyed by Panel::sourceFile, which is unique per descriptor.
		std::map<std::string, std::shared_ptr<PanelFiles>> _panels;
		bool _loaded{ false };
	};
}
