#pragma once

// M3 - Apocrypha Menu Framework registration (plan: 4. plans\wheeler-refined\plan.md).
//
// Wheeler's settings page is drawn in WHEELER's ImGui context (RenderManager::draw), never in
// AMF's - two contexts each own a DX11/Win32 backend and only one may own a frame. So AMF is used
// for exactly two things: the entry in its Mod Control Panel, and a launch. The AMF-side render
// callback draws a short explanation and one button inside AMF's context, through AMF's own
// exported cimgui functions; pressing the button ARMS a launch, and Tick() - called every frame from
// Wheeler's draw - opens the page once AMF reports no blocking window open. That hand-over is
// deliberate: if the page opened while AMF's menu was still up, both overlays would translate the
// same input events (AMF's blocking window and PageInput both consume), and the player would be
// typing into two menus at once.
//
// This file talks to AMF through GetProcAddress only. No AMF or SKSE Menu Framework header is
// vendored: the fork is GPL-3 and AMF is MIT, and an exported-C boundary keeps the two apart
// (plan, "Licence position"). Every pointer is resolved once and the registration is refused
// unless all of them resolve - the framework's own consumer header calls unresolved pointers and
// jumps to address zero (LOGIC-LIBRARY entry 49), so the guard here fails CLOSED.
//
// Degrades silently: with no framework loaded, nothing registers and the page stays reachable by
// its own key (Control.Wheel/SettingsPageKey).
namespace AMFLaunch
{
	// Call once at kDataLoaded (AMF registers pages at any time; kDataLoaded is when Wheeler's own
	// page exists to be launched). Safe to call again; it does nothing after the first success.
	void Register();

	// Call every frame from Wheeler's own draw, before SettingsPage::Page::Draw(). Costs one
	// boolean test per frame while nothing is armed.
	void Tick();

	// For the devbench tool and the log: is a launch armed and waiting for AMF's menu to close?
	bool IsLaunchPending();

	// What the button does, callable from the devbench tool so the launch path can be driven
	// headlessly. The real button is still the thing to click - see GetButtonRect.
	void ArmLaunch();

	// Display-space rectangle of the button as AMF last drew it (all zero until it has been drawn
	// at least once). Lets amf.menu's click op press the REAL button rather than a stand-in.
	struct Rect { float x0 = 0, y0 = 0, x1 = 0, y1 = 0; };
	Rect GetButtonRect();

	// Was the page registered with a framework this session (and under which module name)?
	const char* RegisteredWith();
}
