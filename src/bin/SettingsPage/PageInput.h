#pragma once

#include <cstdint>

namespace RE
{
	class InputEvent;
}

// Input for the settings page.
//
// Wheeler's ImGui context receives no mouse buttons and no keyboard: RenderManager's WndProc hook
// forwards nothing to ImGui, and nothing feeds io.Add*Event by hand. That is correct for the wheel,
// which reads only a cursor angle and takes its activations through Wheeler's own dispatcher - but
// it means every widget on the settings page renders and responds to nothing.
//
// This is the translation step that fixes it. It is NOT a new hook: Wheeler already owns the exact
// call site AMF uses for the same purpose - RELOCATION_ID(67315, 68617) + 0x7B, installed as
// OnInputEventDispatch - so the events are already passing through Input::ProcessAndFilter. All
// that is missing is copying them into ImGui's queue.
//
// THREADING. Events arrive on the game's input thread; ImGui may only be fed from the render
// thread. Feeding io.Add*Event from the input thread while the render thread is inside NewFrame is
// a data race, so records are queued under a lock and drained in the present hook.
namespace SettingsPage
{
	namespace PageInput
	{
		// ---- input thread ----------------------------------------------------------------
		// Called for each event in Input::ProcessAndFilter's walk while the page is open.
		// Copies what ImGui needs and answers whether the GAME should be denied this event.
		//
		// The consume rule is deliberately not "swallow everything". A release passes through when
		// the game already saw its press - AMF learned this the hard way: Skyrim fires shouts on
		// button RELEASE, so a shout key pressed inside a menu was consumed on the down-edge and
		// still completed as a shout on the up-edge. Swallowing a press without its release leaves
		// the game with a key stuck down, which is the opposite failure. Both are avoided by
		// tracking which buttons the game actually believes are held.
		// a_dispatchCode is ProcessAndFilter's post-offset code for this event (its `spyMappedInput`),
		// passed in rather than recomputed here. Recomputing would put the device-offset table in two
		// places, and the two drifting apart mis-binds silently - a wrong keymap, not a crash.
		// 0 for any event that is not a button, which the capture path rejects anyway.
		bool CaptureAndShouldConsume(const RE::InputEvent* a_event, std::uint32_t a_dispatchCode);

		// Called for EVERY event at the point Wheeler's consume decision is final - whether the
		// page is open or not - so the record of what the GAME believes is held stays truthful.
		//
		// This must not be folded into CaptureAndShouldConsume. That function only runs while the
		// page is open, and while it is open every press is consumed, so the held set would never
		// gain an entry: it would be permanently empty, every release would be swallowed, and a key
		// held across the moment the page opens would never get its release - a stuck key, which is
		// the exact failure the set exists to prevent.
		void ObserveOutcome(const RE::InputEvent* a_event, bool a_consumed);

		// ---- render thread ---------------------------------------------------------------
		// Drains the queue into ImGui. MUST be called between the backend NewFrame calls and
		// ImGui::NewFrame(), and it publishes one authoritative cursor position per frame as its
		// last act - the Win32 backend's fallback poll pushes the OS cursor (which the game
		// recentres at will) into the same queue every frame, and on a silent frame that stale
		// position would otherwise win.
		void ProcessQueuedEvents();

		// Sampled inside the frame so the controller scheme knows whether a value is being edited:
		// nothing held -> the left stick navigates; an item held -> the right stick moves the value.
		// Exactly one stick drives ImGui's nav axes per frame, and the idle one is explicitly
		// released so a resting off-centre stick cannot leave an axis stuck down.
		void SetItemActive(bool a_active);

		// Clears the queue and any half-held state, so a key held across the transition cannot
		// leak into the page - or out of it.
		void OnPageOpened();
		void OnPageClosed();

		// Absolute cursor position in display space, for diagnostics.
		void GetCursor(float& a_x, float& a_y);

		// ---- keymap capture ----------------------------------------------------------------
		// Wheeler's own Controls::BeginRebind cannot serve the page: its RebindTarget enum has five
		// values, all Ammo Wheel, and PollInputDevices returns early for anything else - so 57 of
		// the 62 keymap controls have no capture path. This is that path.
		//
		// The code captured is the POST-OFFSET dispatch code (ProcessAndFilter's `input`), NOT
		// button->GetIDCode(). That is what the INI actually stores, for every device: mouse is
		// +256 (activatePrimary = 256 is left mouse), gamepad is an index above KEY_GAMEPAD_OFFSET
		// = 266 (toggleWheel = 278 is "X"), keyboard is the raw scan code because its offset is 0.
		// Capturing the raw id instead would silently mis-bind every mouse and gamepad row.

		// Arm capture for one keymap. Only one may be armed at a time; arming again replaces it.
		void BeginKeymapCapture();
		void CancelKeymapCapture();
		bool IsCapturingKeymap();

		// Non-zero once a key has been captured, then cleared by taking it. 0 means nothing yet.
		// Returns the post-offset dispatch code, ready to write straight into the INI.
		std::uint32_t TakeCapturedKey();

		// True when the player pressed a cancel key (Escape or Home) - so the page can drop back out
		// without binding anything. Controls.cpp has the same test as a FILE-LOCAL static, not a
		// member of Controls, so it cannot be called from here; the two are kept in step by hand.
		bool TakeCaptureCancelled();

		// ---- driving (DevBench) ------------------------------------------------------------
		// Supplies a captured code WITHOUT a physical key press, so the rebind path can be driven
		// from the devbench tool instead of needing someone at the keyboard.
		//
		// It runs the REAL capture path - the armed check, the 0/kInvalid rejection and the cancel
		// branch all still apply - and drops only the 250 ms grace window, which exists to stop the
		// click that armed capture being captured itself. A driven injection has no such click, and
		// without clearing the grace an immediate inject would simply be swallowed.
		//
		// So this proves everything downstream of capture (take -> ValueStore::Set ->
		// BindAllInputsFromConfig) and everything in capture except the timing guard. What it does
		// NOT exercise is the ProcessAndFilter splice that supplies the post-offset code in a real
		// press; that part still needs a human or a synthesised button event.
		//
		// Returns true if capture consumed the injection (armed, and not rejected).
		bool InjectCapturedKey(std::uint32_t a_dispatchCode, bool a_isCancelKey);
	}
}
