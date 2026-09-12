#pragma once

#include "Descriptor.h"
#include "ValueStore.h"

#include <string>

// The part of the settings page that can be decided without drawing anything.
//
// It lives apart from the ImGui renderer on purpose: requirement evaluation, the preset guard and
// value formatting are the places a settings page actually goes wrong, and all three can be proved
// headlessly against the real descriptors and the real INI files. The renderer then becomes a thin
// shell over decisions that have already been checked.
namespace SettingsPage
{
	enum class Visibility
	{
		Shown,     // requirements met
		Disabled,  // requirements not met, failAction = disable (931 of 943)
		Hidden     // requirements not met, failAction = hide (12 of 943)
	};

	const char* VisibilityName(Visibility a_visibility);

	// Evaluates `control.requirements` against the values currently in the INI files. Each
	// requirement names ANOTHER entry by its `control.id` and the value it must hold.
	//
	// A requirement whose target cannot be resolved is treated as SATISFIED rather than as a
	// failure: refusing to show a control because of a descriptor typo would hide a working setting
	// from the player. (Measured: 0 of 286 currently dangle, so this is a guard, not a crutch.)
	Visibility EvaluateVisibility(
		const Catalog& a_catalog,
		const Panel& a_panel,
		const Entry& a_entry,
		const ValueStore& a_store);

	// ---- the preset trap -------------------------------------------------------------------
	// Wheeler's `applyPresetRGB` returns early only when the preset is 0 ("Custom"); for any other
	// value it OVERWRITES r/g/b from a palette. So a colour whose section has a sibling `<key>Preset`
	// holding non-zero is inert - editing it changes nothing on screen. The descriptor does not
	// encode this (0 of 8 colour controls are guarded by a preset requirement), so the page adds the
	// condition itself, or a player edits a colour and reports the page as broken.

	// "BorderColor" -> "BorderColorPreset". Empty for non-colour entries.
	std::string PresetKeyFor(const Entry& a_entry);

	// True when a sibling preset key exists in the same section AND is non-zero.
	bool IsOverriddenByPreset(const Panel& a_panel, const Entry& a_entry, const ValueStore& a_store);

	// ---- writing values back ---------------------------------------------------------------
	// Every formatter takes the CURRENT resolved value and matches the notation it finds, because
	// the shipped files are not consistent with themselves: "0.200000" and "1.0" and "278" and
	// "true" all occur. Rewriting `1.0` as `1.000000` would churn a file the player never changed.

	std::string FormatNumberLike(double a_value, const ResolvedValue& a_current);
	std::string FormatBoolLike(bool a_value, const ResolvedValue& a_current);
	std::string FormatColorLike(const ColorValue& a_value, const ResolvedValue& a_current);
	std::string FormatIndexLike(int a_value, const ResolvedValue& a_current);
}
