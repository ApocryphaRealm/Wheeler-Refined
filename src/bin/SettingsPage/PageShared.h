#pragma once

// M9 (the owner, 2026-09-13: "All those wheeler controls should be inside of AMF in its own wheeler
// mod menu"). The settings page is drawn in TWO places by two ImGui surfaces - Wheeler's own overlay
// (Page.cpp, Wheeler's ImGui context, the fallback when no framework is loaded) and the Apocrypha
// Menu Framework's menu (AmfPage.cpp, the framework's exported ImGui, the real home). Everything
// that is NOT drawing - capture, the row notices, the button actions, the settings reload - lives in
// Page.cpp and is reached from the framework page through these wrappers, so the two surfaces can
// never drift apart on what a rebind or a button DOES.

#include "Descriptor.h"
#include "ValueStore.h"

#include <cstdint>
#include <string>

namespace SettingsPage::Page::Shared
{
	enum class CaptureOutcome
	{
		NotPending,
		Waiting,
		Cancelled,
		Bound,
		Reserved,
		InUse,
		WrongDevice,
		WriteFailed
	};

	// Identifies a row for capture and notices: section + "/" + key. The key alone is NOT unique -
	// "toggleWheel" is in both InputBindings.MKB and InputBindings.GamePad - and keying by it armed
	// both rows at once (the owner, 2026-09-15).
	std::string CaptureId(const Entry& a_entry);

	// Arms the row (PageInput capture + the row bookkeeping), exactly as the overlay's Rebind button does.
	void BeginCapture(const std::string& a_iniKey);
	void CancelCapture();
	bool IsCapturing(const std::string& a_iniKey);
	bool IsAnyCapturing();

	// Runs the overlay's ConsumeCapture for this row: reserved-key and one-key-one-action checks,
	// the INI write, the rebind. a_boundCode receives the code on Bound.
	CaptureOutcome ConsumeCapture(const Panel& a_panel, const Entry& a_entry,
		const ResolvedValue& a_current, std::uint32_t& a_boundCode);

	// Clears a keymap row to 0 ("Unbound"), through the same write and dispatcher rebuild a capture
	// uses - what the Unbind button beside Rebind does (the owner, 2026-09-15).
	bool Unbind(const Panel& a_panel, const Entry& a_entry, const ResolvedValue& a_current);

	// The sticky notice for a row ("not bound - ..."), empty when none.
	std::string RowNotice(const std::string& a_iniKey);
	void ClearRowNotice(const std::string& a_iniKey);

	bool LooksLikeGamepad(const Entry& a_entry);

	// Descriptor buttons: is the id wired, and run it.
	bool IsKnownButton(const std::string& a_id);
	void RunButtonAction(const std::string& a_id);

	// After a value write: the lightweight reload the overlay does.
	void ApplyChangedSettings();
}
