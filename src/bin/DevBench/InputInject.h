#pragma once

#include <cstdint>

// Driving-tool input injection (rule 64), for the proof of the wheel's input rules.
//
// WHY THIS EXISTS INSIDE WHEELER: Wheeler, Apocrypha Menu Framework and InputBench all hook the same
// PollInputDevices -> DispatchInputEvent call site (RELOCATION_ID 67315/68617 + 0x7B). The LAST plugin
// to install wraps the others, and wheeler.dll loads after InputBench.dll, so Wheeler's filter runs on
// the RAW event list before InputBench has prepended its synthetic presses. An InputBench press
// therefore never reaches Wheeler's filter at all - measured 2026-09-12: zero spy lines, wheel never
// opened, while InputBench's own log showed every splice. Splicing here, at the head of Wheeler's own
// hook, puts the event in front of Wheeler's filter AND on to every downstream handler and the game,
// which is exactly what a hardware press looks like from this point on.
//
// Mechanism mirrors InputBench (ours, MIT): a queue of presses, each spliced as a DOWN, held for N
// dispatches with a rising held-time, then an UP; the user event name is resolved from the control
// map the way the engine does it.
namespace InputInject
{
	// device: 0 keyboard, 1 mouse, 2 gamepad (RE::INPUT_DEVICE values). code: DirectInput scan code /
	// XInput mask / mouse button. holdFrames clamped 1..600. Thread-safe (called from devbench's
	// listener thread); consumed on the game's input thread.
	// a_replay = true marks the press as a REPLAY (M8): a tap Wheeler swallowed while deciding
	// hold-or-tap, now handed to the game. Wheeler's own filter skips replay events (IsReplay) so the
	// replayed D-pad press cannot arm a second hold.
	void QueuePress(std::uint32_t a_device, std::uint32_t a_code, int a_holdFrames, bool a_replay = false);

	// True for an event this module spliced as a replay during the CURRENT dispatch. Valid only
	// between Service() and the end of the same Input::ProcessAndFilter call.
	bool IsReplay(const RE::InputEvent* a_event);

	// Called by Hooks::OnInputEventDispatch BEFORE Input::ProcessAndFilter.
	void Service(RE::InputEvent** a_events);

	std::uint32_t Pending();
}
