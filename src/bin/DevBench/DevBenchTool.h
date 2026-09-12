#pragma once

// The DevBench DRIVING tool for Wheeler's settings page.
//
// House rule: every mod with an interactive surface ships a tool that lets the surface be operated
// without a person at the keyboard. Without it an in-game test of the page degrades into asking the
// owner to press F11, click Rebind and hit a key - which is the work the autonomous test loop exists
// to take over.
//
// Registers "wheeler.page" with devbench when devbench is present. It is entirely optional: if
// devbench is not installed, GetDevBenchInterface001() returns nullptr, nothing is registered, and
// Wheeler behaves exactly as it does today.
//
// THREADING. devbench invokes tool handlers on its own listener thread. Anything touching the page's
// render-thread state goes through SettingsPage::Page's Drive* entry points, which queue the request
// and wait for the render thread to run it.
namespace DevBenchTool
{
	// Call with false at kPostLoad and true at kDataLoaded - the two-phase pattern the other mods in
	// this project use. devbench may not have finished loading at the first attempt, so the second
	// is a retry; the `a_lastAttempt` flag only decides whether its absence is worth a log line.
	void Init(bool a_lastAttempt);
}
