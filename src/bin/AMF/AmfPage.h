#pragma once

// M9 - Wheeler's settings as a mod menu INSIDE the Apocrypha Menu Framework (the owner, 2026-09-13:
// "All those wheeler controls should be inside of AMF in its own wheeler mod menu").
//
// Every descriptor panel becomes one section of the "Wheeler - Refined" entry in the framework's
// menu (AddSectionItem, the SKSE Menu Framework consumer path AMF implements), and the panel's tabs
// and controls are drawn through the framework's exported ImGui inside ITS context, wearing its
// theme. The values, the capture, the reserved-key and one-key-one-action refusals and the button
// actions are the same code the overlay page uses (SettingsPage::Page::Shared), so the two surfaces
// cannot disagree on what a control does.
//
// Licence position: the fork is GPL-3; the framework is reached through the MIT consumer header
// vendored at bin/AMF/SKSEMenuFramework.h (SKSE Menu Framework 3, MIT), which resolves exports by
// name - nothing of AMF's is linked or copied.
//
// Degrades silently: with no framework loaded nothing registers and the overlay page (Page.cpp)
// stays reachable by its own key.
namespace AmfPage
{
	// Call once at kDataLoaded, after the descriptors are parsed. Safe to call again.
	void Register();

	// True once the sections are registered with a framework: the settings live there, and the
	// overlay page is only a fallback.
	bool IsHosted();

	// Module name the sections were registered with, "" when none.
	const char* HostedBy();
}
