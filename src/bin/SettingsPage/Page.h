#pragma once

#include <cstdint>
#include <string>

// The drawn settings page: a thin shell over PageModel (what the tabs are), ValueStore (what the
// values are) and PageLogic (what is enabled, and how a value is written back). Everything that can
// be decided without drawing lives in those three; this file only turns decisions into widgets.
//
// Drawn from RenderManager::draw(), inside Wheeler's own ImGui context - not AMF's. Two contexts
// with their own DX11/Win32 backends already exist in this process and only one may own a frame.
namespace SettingsPage
{
	namespace Page
	{
		// Costs one boolean test per frame while closed.
		void Draw();

		bool IsOpen();
		void SetOpen(bool a_open);
		void Toggle();

		// Reads [SettingsPage] ShowOnLoad from Data\SKSE\Plugins\wheeler\debug.ini and opens the page
		// at data load when it is true.
		//
		// It originally existed because nothing could open the page at all. That is no longer so -
		// the settings-page key (Control.Wheel/SettingsPageKey, default 0x57 = F11) opens and closes
		// it, and PageInput feeds the ImGui context - so this is now only a convenience for looking
		// at the page without pressing anything. Kept because it costs one INI read at startup.
		void LoadStartupFlag();

		// ---- driving (DevBench) --------------------------------------------------------------
		// Called from devbench's LISTENER thread, never the render thread. Everything below is safe
		// to call from any thread: a request is queued and drained at the top of Draw().
		//
		// The drain sits ABOVE Draw()'s early-out for a closed page. That is not incidental - if it
		// sat below, a request made while the page was shut (including the request to OPEN it) would
		// never run, which is precisely the state the tool is used from.
		//
		// Each call blocks until the render thread has run the request, or until a_timeoutMs passes.
		// Blocking without a deadline would hang devbench's listener thread whenever rendering
		// stalls - a paused game, a load screen, a modal - so a timeout is reported as a failure
		// rather than waited out.

		// The one-line summary the tool reports: open state, whether the catalogue and tab model are
		// built, and how many controls resolved. Safe without the queue - all of it is atomic or
		// immutable after load.
		std::string DriveStatusJson();

		// Selects a panel tab and, optionally, a tab within it by label. Empty a_tab leaves the
		// inner selection alone. Honoured on the next frame via ImGuiTabItemFlags_SetSelected,
		// because ImGui owns tab selection and there is no other way in.
		bool DriveSelectTab(const std::string& a_panelLabel, const std::string& a_tabLabel,
			std::string& a_resultJson, int a_timeoutMs = 2000);

		// Reads one control by its INI key, reporting the raw text AND its provenance (shipped
		// default versus the player's own override), which is what makes a driven read legible.
		bool DriveGet(const std::string& a_iniKey, std::string& a_resultJson, int a_timeoutMs = 2000);

		// Writes one control by its INI key, through the same ValueStore::Set the widgets use - so
		// the read-back verification and the MO2 masking check both still apply - then runs the
		// page's ordinary apply sequence.
		bool DriveSet(const std::string& a_iniKey, const std::string& a_rawValue,
			std::string& a_resultJson, int a_timeoutMs = 2000);

		// Drives the whole keymap rebind path for one control: arms capture on that row, injects the
		// code, and lets the next frame take it, write it and rebind. a_cancel exercises the cancel
		// branch instead of binding.
		bool DriveRebind(const std::string& a_iniKey, std::uint32_t a_dispatchCode, bool a_cancel,
			std::string& a_resultJson, int a_timeoutMs = 2000);

		// The tab tree and the controls on a tab. Answered directly on the calling thread: the
		// Catalog and PageModel are never mutated after load, so no queue is needed.
		std::string DriveTabsJson();
		std::string DriveListJson(const std::string& a_panelLabel, const std::string& a_tabLabel);
	}
}
