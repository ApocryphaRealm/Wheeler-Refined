#pragma once

// Settings presets (the owner, 2026-09-13: "add the default settings ini and user preset ini with
// renameability and several to save to"; the same day: "we dont need cosave specific preset ini
// files ... they are not save specific").
//
// Two parts, all plain files, global to the install - nothing is written into a save:
//   * the shipped defaults - X.defaults.ini / wheelBehavior.factory.ini - never rewritten; "Reset to
//     defaults" copies them over the live user INIs;
//   * user presets - a folder per preset under Data\SKSE\Plugins\wheeler\user\presets\<name>\ holding a
//     copy of every live settings INI; save, load, rename, delete.
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

	// The preset the panel has selected (empty = none): the last one saved or loaded this session.
	// Session state only - it is never written to a save or a file.
	void SetSelected(const std::string& a_name);
	std::string Selected();
	void ClearSelected();
	std::string LastApplied();

	// Shared page state: the name box and the sticky one-line notice under the panel.
	char* NameBuffer();
	std::size_t NameBufferSize();
	std::string Notice();
	void SetNotice(std::string a_notice);

	// For the DevBench driving op: the list, the selected preset, the last applied one.
	std::string StatusJson();
}
