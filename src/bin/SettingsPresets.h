#pragma once

// Settings presets (the owner, 2026-09-13: "add the default settings ini and user preset ini with
// renameability and several to save to, that is save specific so different saves can have different
// settings ini activated").
//
// Three parts, all plain files:
//   * the shipped defaults - X.defaults.ini / wheelBehavior.factory.ini - never rewritten; "Reset to
//     defaults" copies them over the live user INIs;
//   * user presets - a folder per preset under Data\SKSE\Plugins\wheeler\user\presets\<name>\ holding a
//     copy of every live settings INI; save, load, rename, delete;
//   * the preset in force for THIS SAVE - its name travels in Wheeler's own co-save record, so loading
//     a save that names a preset applies it (once per session per preset: switching saves switches
//     presets, reloading the same save keeps the tweaks made since).
//
// The live user INIs stay the thing the settings page edits; a preset is a snapshot of them.
// Both settings surfaces (the overlay page and the framework-hosted page) draw the same panel
// from the state kept here, so they cannot disagree about what is selected or what just happened.

#include <cstddef>
#include <string>
#include <vector>

namespace SettingsPresets
{
	struct IniFile
	{
		const char* label;
		const char* userPath;      // the live INI the settings page edits
		const char* defaultsPath;  // the shipped file it was seeded from
	};

	const std::vector<IniFile>& Files();
	const char* Root();

	std::vector<std::string> List();
	bool Exists(const std::string& a_name);
	bool IsValidName(const std::string& a_name, std::string& a_reason);

	// Every operation reports through a_err on failure and returns false; nothing throws.
	bool SaveAs(const std::string& a_name, std::string& a_err);
	bool Load(const std::string& a_name, std::string& a_err);
	bool Rename(const std::string& a_from, const std::string& a_to, std::string& a_err);
	bool Delete(const std::string& a_name, std::string& a_err);
	bool ResetToDefaults(std::string& a_err);
	void ReloadFromIni();

	// The preset recorded for the loaded save (empty = none). Set by the co-save reader and by the
	// page; cleared by the revert callback (new game, or a save without Wheeler data).
	void SetSavePreset(const std::string& a_name);
	std::string SavePreset();
	void ClearSavePreset();
	std::string LastApplied();

	// kPostLoadGame: apply the save's preset if it names one that exists and is not already in force.
	void ApplySavePresetOnLoad(const char* a_reason);

	// Shared page state: the name box and the sticky one-line notice under the panel.
	char* NameBuffer();
	std::size_t NameBufferSize();
	std::string Notice();
	void SetNotice(std::string a_notice);

	// For the DevBench driving op: the list, the save's preset, the last applied one.
	std::string StatusJson();
}
