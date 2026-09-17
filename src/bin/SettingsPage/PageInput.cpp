#include "PageInput.h"
#include "bin/UserInput/LeftStick.h"

#include "Page.h"

#include "imgui.h"

// For DIK_ESCAPE / DIK_HOME. Controls.cpp takes the same codes from the same header; the cancel
// test below mirrors its file-local IsCancelMkbKey, which is not reachable from this file.
#include <dinput.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <unordered_set>
#include <vector>

namespace SettingsPage
{
	namespace PageInput
	{
		// 1.3.1: declared before the anonymous namespace as well, so CaptureLeftStickFlick inside it can hand an armed
		// row a left-stick flick (LeftStick.h). Same function as the declaration further down.
		bool BeginCaptureFromInputThread(std::uint32_t a_dispatchCode, bool a_isCancelKey);
		namespace
		{
			// A flat POD so the input thread copies and moves on - no allocation, no ImGui call,
			// nothing that can block the game's input dispatch.
			struct Record
			{
				enum class Kind : std::uint8_t
				{
					kMouseMove,
					kMouseButton,
					kMouseWheel,
					kKeyboard,
					kGamepad,
					kThumbstick,
					kCharacter,
				};

				Kind kind{};
				std::uint32_t code = 0;  // mouse button index / DIK scancode / XInput mask / unicode
				bool down = false;       // transitions only
				float x = 0.0f;
				float y = 0.0f;
			};

			std::mutex g_queueLock;
			std::vector<Record> g_queue;

			// Buttons the GAME currently believes are held, maintained on the input thread.
			// Keyed device << 32 | idCode.
			//
			// This set is the whole reason a release is not passed unconditionally. Skyrim fires
			// shouts on button RELEASE: a shout key pressed inside the page would be consumed on
			// the down-edge and still complete as a shout on the up-edge. The opposite error -
			// swallowing a press without its release - leaves the game with a key stuck down. Only
			// tracking what the game actually saw avoids both.
			std::unordered_set<std::uint64_t> g_gameHeldButtons;

			// Reset requested, honoured on the INPUT thread.
			//
			// OnPageOpened/OnPageClosed can now be reached from either thread - the toggle key
			// arrives on the input thread, but the title-bar close button runs inside Draw on the
			// render thread. Clearing the set from there would race ObserveOutcome's insert/erase,
			// and a concurrent clear() against insert() on an unordered_set is undefined behaviour.
			//
			// A flag keeps the set strictly input-thread-owned, which is its stated invariant, and
			// costs the hot path nothing. A close with no following input just leaves a stale set
			// that nothing reads until the next event.
			std::atomic<bool> g_resetHeldPending{ false };

			// Software cursor in display space, owned by the render thread. Integrated from raw
			// MouseMoveEvent deltas.
			//
			// NOT from Wheeler's resolutionContext.ToGameX/ToGameY: those convert display pixels
			// into the wheel's GAME space for its polar maths. ImGui works in display space and
			// clamps against io.DisplaySize, so scaled deltas would drift against the screen.
			float g_cursorX = 0.0f;
			float g_cursorY = 0.0f;
			// The floats stay render-thread-only, but this flag crosses: OnPageOpened clears it on
			// the INPUT thread while ProcessQueuedEvents reads and sets it on the RENDER thread.
			std::atomic<bool> g_cursorPrimed{ false };

			std::atomic<bool> g_itemActive{ false };

			// ---- keymap capture state ----------------------------------------------------
			// All of it crosses threads: armed and read on the RENDER thread (the page), consumed
			// on the INPUT thread (ProcessAndFilter). The same hazard already bit g_open and
			// g_gameHeldButtons in this file's history, so these are atomic from the start rather
			// than after a symptom.
			std::atomic<bool> g_captureArmed{ false };
			std::atomic<bool> g_captureCancelled{ false };
			std::atomic<std::uint32_t> g_capturedKey{ 0u };

			// A steady_clock::time_point is not trivially atomic, so the deadline is held as a
			// count of ticks instead. Written when arming, read on every event until it passes.
			std::atomic<std::int64_t> g_captureIgnoreUntilTicks{ 0 };

			bool WithinCaptureGrace()
			{
				const auto until = g_captureIgnoreUntilTicks.load(std::memory_order_acquire);
				return std::chrono::steady_clock::now().time_since_epoch().count() < until;
			}

			void SetCaptureGrace(std::chrono::milliseconds a_window)
			{
				const auto deadline = (std::chrono::steady_clock::now() + a_window).time_since_epoch().count();
				g_captureIgnoreUntilTicks.store(deadline, std::memory_order_release);
			}

			constexpr float kStickDeadzone = 0.35f;
			constexpr std::size_t kMaxQueued = 512;

			void Enqueue(const Record& a_record)
			{
				std::scoped_lock lock(g_queueLock);
				// A runaway queue means the render thread stopped draining (device lost, game
				// paused oddly). Dropping input beats growing without bound on the input thread.
				if (g_queue.size() < kMaxQueued) {
					g_queue.push_back(a_record);
				}
			}

			std::uint64_t ButtonKey(const RE::ButtonEvent* a_button)
			{
				return (static_cast<std::uint64_t>(a_button->GetDevice()) << 32) | a_button->GetIDCode();
			}

			// DIK scancode -> ImGuiKey, navigation and editing. Text arrives separately as
			// CharEvent -> AddInputCharacter, so this table does not need letter keys.
			// Table taken from our own Apocrypha Menu Framework (MIT), which solved the identical
			// problem in this engine.
			ImGuiKey ScancodeToImGuiKey(std::uint32_t a_scancode)
			{
				switch (a_scancode) {
				case 0x01: return ImGuiKey_Escape;
				case 0x0F: return ImGuiKey_Tab;
				case 0x1C: return ImGuiKey_Enter;
				case 0x39: return ImGuiKey_Space;
				case 0x0E: return ImGuiKey_Backspace;
				case 0xD3: return ImGuiKey_Delete;
				case 0xC8: return ImGuiKey_UpArrow;
				case 0xD0: return ImGuiKey_DownArrow;
				case 0xCB: return ImGuiKey_LeftArrow;
				case 0xCD: return ImGuiKey_RightArrow;
				case 0xC7: return ImGuiKey_Home;
				case 0xCF: return ImGuiKey_End;
				case 0xC9: return ImGuiKey_PageUp;
				case 0xD1: return ImGuiKey_PageDown;
				case 0x2A: return ImGuiKey_LeftShift;
				case 0x36: return ImGuiKey_RightShift;
				case 0x1D: return ImGuiKey_LeftCtrl;
				case 0x9D: return ImGuiKey_RightCtrl;
				case 0x38: return ImGuiKey_LeftAlt;
				case 0xB8: return ImGuiKey_RightAlt;
				default:   return ImGuiKey_None;
				}
			}

			// XInput button mask (RE::BSWin32GamepadDevice::Key) -> ImGuiKey gamepad navigation.
			ImGuiKey GamepadMaskToImGuiKey(std::uint32_t a_mask)
			{
				switch (a_mask) {
				case 0x0001: return ImGuiKey_GamepadDpadUp;
				case 0x0002: return ImGuiKey_GamepadDpadDown;
				case 0x0004: return ImGuiKey_GamepadDpadLeft;
				case 0x0008: return ImGuiKey_GamepadDpadRight;
				case 0x1000: return ImGuiKey_GamepadFaceDown;   // A - activate
				case 0x2000: return ImGuiKey_GamepadFaceRight;  // B - cancel
				case 0x4000: return ImGuiKey_GamepadFaceLeft;
				case 0x8000: return ImGuiKey_GamepadFaceUp;
				case 0x0100: return ImGuiKey_GamepadL1;
				case 0x0200: return ImGuiKey_GamepadR1;
				default:     return ImGuiKey_None;
				}
			}

			// A flick past the press threshold while a row is armed binds that direction; latched until the stick returns.
			bool CaptureLeftStickFlick(const RE::ThumbstickEvent* a_thumb)
			{
				static bool s_flickLatched = false;
				if (!a_thumb || !a_thumb->IsLeft() || !IsCapturingKeymap()) { return false; }
				const std::uint32_t dir = LeftStick::CodeFor(a_thumb->xValue, a_thumb->yValue);
				if (dir == 0) { s_flickLatched = false; return false; }
				if (s_flickLatched) { return false; }
				s_flickLatched = true;
				return BeginCaptureFromInputThread(dir, false);
			}

			void CopyForImGui(const RE::InputEvent* a_event)
			{
				switch (a_event->GetEventType()) {
				case RE::INPUT_EVENT_TYPE::kMouseMove:
					{
						const auto* move = static_cast<const RE::MouseMoveEvent*>(a_event);
						Enqueue({ Record::Kind::kMouseMove, 0, false,
							static_cast<float>(move->mouseInputX), static_cast<float>(move->mouseInputY) });
						break;
					}
				case RE::INPUT_EVENT_TYPE::kButton:
					{
						const auto* button = static_cast<const RE::ButtonEvent*>(a_event);

						// Transitions only. ImGui tracks held state itself, and the raw
						// held-repeat frames (value > 0, heldDownSecs > 0) would double-fire.
						const bool isDown = button->IsDown();
						const bool isUp = button->IsUp();
						if (!isDown && !isUp) {
							break;
						}

						switch (button->GetDevice()) {
						case RE::INPUT_DEVICE::kMouse:
							// The engine delivers the wheel as button 8 (up) / 9 (down).
							if (button->GetIDCode() == 8 || button->GetIDCode() == 9) {
								if (isDown) {
									Enqueue({ Record::Kind::kMouseWheel, 0, false, 0.0f,
										button->GetIDCode() == 8 ? 1.0f : -1.0f });
								}
							} else if (button->GetIDCode() <= 4) {
								Enqueue({ Record::Kind::kMouseButton, button->GetIDCode(), isDown, 0.0f, 0.0f });
							}
							break;
						case RE::INPUT_DEVICE::kKeyboard:
							Enqueue({ Record::Kind::kKeyboard, button->GetIDCode(), isDown, 0.0f, 0.0f });
							break;
						case RE::INPUT_DEVICE::kGamepad:
							Enqueue({ Record::Kind::kGamepad, button->GetIDCode(), isDown, 0.0f, 0.0f });
							break;
						default:
							break;
						}
						break;
					}
				case RE::INPUT_EVENT_TYPE::kChar:
					{
						const auto* character = static_cast<const RE::CharEvent*>(a_event);
						// `keycode`, lower-case c: that is how CommonLibSSE-NG spells it at this
						// revision (RE/C/CharEvent.h). AMF's copy of this line reads `keyCode`
						// because it builds against a different CommonLib - and Wheeler's own
						// Input.cpp carries a local CharEvent shim for the same reason, which is
						// what would have made a compile error here look mysterious.
						Enqueue({ Record::Kind::kCharacter, character->keycode, true, 0.0f, 0.0f });
						break;
					}
				case RE::INPUT_EVENT_TYPE::kThumbstick:
					{
						const auto* thumb = static_cast<const RE::ThumbstickEvent*>(a_event);
						// 1.3.1: an armed keymap row takes a left-stick flick as one of the four direction buttons
						// (LeftStick.h), so Move Wheel Forward can be put back on the stick from the page.
						if (CaptureLeftStickFlick(thumb)) { break; }
						Enqueue({ Record::Kind::kThumbstick, thumb->IsLeft() ? 0u : 1u, false,
							thumb->xValue, thumb->yValue });
						break;
					}
				default:
					break;
				}
			}
		}

		// Defined further down, beside the rest of the capture logic. Declared here because its only
		// caller sits above it, and it is deliberately NOT in the header: the input thread is the
		// only thing allowed to drive capture, and this file owns that path.
		bool BeginCaptureFromInputThread(std::uint32_t a_dispatchCode, bool a_isCancelKey);

		bool CaptureAndShouldConsume(const RE::InputEvent* a_event, std::uint32_t a_dispatchCode)
		{
			if (!a_event) {
				return false;
			}

			// M9: a keymap row armed on the framework-hosted page captures here too, with the overlay
			// closed - the capture bookkeeping is the same, only the widget that armed it differs.
			if (!Page::IsOpen()) {
				if (a_event->eventType == RE::INPUT_EVENT_TYPE::kThumbstick && CaptureLeftStickFlick(static_cast<const RE::ThumbstickEvent*>(a_event))) {
					return true;   // 1.3.1: the flick bound the row
				}
				const auto* hostedButton = a_event->AsButtonEvent();
				if (hostedButton && hostedButton->IsDown() && IsCapturingKeymap()) {
					const bool isCancel =
						hostedButton->device.get() == RE::INPUT_DEVICE::kKeyboard &&
						(a_dispatchCode == DIK_ESCAPE || a_dispatchCode == DIK_HOME);
					if (BeginCaptureFromInputThread(a_dispatchCode, isCancel)) {
						return true;
					}
				}
				return false;
			}

			CopyForImGui(a_event);

			const auto* button = a_event->AsButtonEvent();

			// ---- keymap capture, decided BEFORE the ordinary consume logic -------------------
			// Down-edge only. A release must never become a binding: the player lets go of the key
			// they just bound, and without this test that release would be captured as a second,
			// overwriting press. The grace window alone does not cover it - a key held longer than
			// the window still delivers its release afterwards.
			if (button && button->IsDown() && IsCapturingKeymap()) {
				// Escape and Home cancel, but only from the KEYBOARD. Today no mouse (+256) or
				// gamepad (+266) code can collide with DIK_ESCAPE (1) or DIK_HOME (199), so the
				// device test is redundant - but that is an accident of the current offsets, not a
				// promise, and a future offset change must not silently turn a gamepad button into
				// a cancel.
				const bool isCancel =
					button->device.get() == RE::INPUT_DEVICE::kKeyboard &&
					(a_dispatchCode == DIK_ESCAPE || a_dispatchCode == DIK_HOME);

				if (BeginCaptureFromInputThread(a_dispatchCode, isCancel)) {
					return true;  // capture owns this event whether or not it bound anything
				}
			}

			if (!button) {
				// Mouse movement, thumbsticks and characters are consumed: this is what stops the
				// camera turning and the view zooming while the page is up.
				return true;
			}

			if (button->IsUp()) {
				// Pass the release ONLY if the game saw the press - i.e. it was held across the
				// transition into the page. A release for a press the game never saw is not a
				// harmless no-op here: release-triggered actions (shouts) would fire from it.
				const auto held = g_gameHeldButtons.find(ButtonKey(button));
				if (held != g_gameHeldButtons.end()) {
					g_gameHeldButtons.erase(held);
					return false;  // let the game complete the release cleanly
				}
				return true;
			}

			// A press consumed here is simply never recorded as held: ObserveOutcome inserts only
			// when the game actually receives the event, so it is the single source of truth.
			return true;
		}

		void ObserveOutcome(const RE::InputEvent* a_event, bool a_consumed)
		{
			// Honoured here because this runs on the input thread for EVERY event, open or closed -
			// the one place guaranteed to see a reset requested from either side.
			if (g_resetHeldPending.exchange(false, std::memory_order_acq_rel)) {
				g_gameHeldButtons.clear();
			}

			if (!a_event) {
				return;
			}

			const auto* button = a_event->AsButtonEvent();
			if (!button) {
				return;
			}

			if (a_consumed) {
				// The game will not see this event, so what it believes is held does not change.
				return;
			}

			if (button->IsDown()) {
				g_gameHeldButtons.insert(ButtonKey(button));
			} else if (button->IsUp()) {
				g_gameHeldButtons.erase(ButtonKey(button));
			}
		}

		void ProcessQueuedEvents()
		{
			std::vector<Record> drained;
			{
				std::scoped_lock lock(g_queueLock);
				drained.swap(g_queue);
			}

			ImGuiIO& io = ImGui::GetIO();
			const ImVec2 display = io.DisplaySize;

			if (!g_cursorPrimed) {
				g_cursorX = display.x * 0.5f;
				g_cursorY = display.y * 0.5f;
				g_cursorPrimed = true;
			}

			for (const Record& record : drained) {
				switch (record.kind) {
				case Record::Kind::kMouseMove:
					g_cursorX += record.x;
					g_cursorY += record.y;
					g_cursorX = (g_cursorX < 0.0f) ? 0.0f : ((g_cursorX > display.x - 1.0f) ? display.x - 1.0f : g_cursorX);
					g_cursorY = (g_cursorY < 0.0f) ? 0.0f : ((g_cursorY > display.y - 1.0f) ? display.y - 1.0f : g_cursorY);
					break;

				case Record::Kind::kMouseButton:
					if (record.code < ImGuiMouseButton_COUNT) {
						io.AddMouseButtonEvent(static_cast<int>(record.code), record.down);
					}
					break;

				case Record::Kind::kMouseWheel:
					io.AddMouseWheelEvent(record.x, record.y);
					break;

				case Record::Kind::kKeyboard:
					{
						const ImGuiKey key = ScancodeToImGuiKey(record.code);
						if (key != ImGuiKey_None) {
							io.AddKeyEvent(key, record.down);
							// Modifier flags are tracked explicitly: ImGui's modifiers are not
							// left/right-side conscious, so the side-specific key alone is not
							// enough for shortcuts or text selection to behave.
							if (key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift) {
								io.AddKeyEvent(ImGuiMod_Shift, record.down);
							} else if (key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl) {
								io.AddKeyEvent(ImGuiMod_Ctrl, record.down);
							} else if (key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt) {
								io.AddKeyEvent(ImGuiMod_Alt, record.down);
							}
						}
						break;
					}

				case Record::Kind::kGamepad:
					{
						const ImGuiKey key = GamepadMaskToImGuiKey(record.code);
						if (key != ImGuiKey_None) {
							io.AddKeyEvent(key, record.down);
						}
						break;
					}

				case Record::Kind::kThumbstick:
					{
						// Exactly one stick drives ImGui's nav axes per frame, because ImGui uses
						// the same axes for moving the selection and for changing a held value:
						// nothing being edited -> the LEFT stick navigates; an item taken hold of
						// -> the RIGHT stick moves it. The idle stick is explicitly released, or a
						// resting off-centre stick leaves a nav axis stuck down.
						const bool editing = g_itemActive.load(std::memory_order_relaxed);
						const bool isLeftStick = (record.code == 0);
						const bool inCharge = editing ? !isLeftStick : isLeftStick;

						const float sx = inCharge ? record.x : 0.0f;
						const float sy = inCharge ? record.y : 0.0f;
						io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft,  sx < -kStickDeadzone, sx < -kStickDeadzone ? -sx : 0.0f);
						io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, sx >  kStickDeadzone, sx >  kStickDeadzone ?  sx : 0.0f);
						io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp,    sy >  kStickDeadzone, sy >  kStickDeadzone ?  sy : 0.0f);
						io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown,  sy < -kStickDeadzone, sy < -kStickDeadzone ? -sy : 0.0f);
						break;
					}

				case Record::Kind::kCharacter:
					io.AddInputCharacter(record.code);
					break;
				}
			}

			// One authoritative cursor position per frame, movement or not, and LAST.
			//
			// ImGui_ImplWin32_NewFrame polls the OS cursor into this same queue every frame, and
			// the game recentres that cursor at will. On a frame where we said nothing, that stale
			// position would win - which is a pointer that flickers or teleports. Publishing
			// unconditionally afterwards makes the software cursor the only position ImGui acts on.
			//
			// Only while the page is open: when it is closed this function is not called at all, so
			// the wheel's use of this context is untouched.
			io.AddMousePosEvent(g_cursorX, g_cursorY);
		}

		void SetItemActive(bool a_active)
		{
			g_itemActive.store(a_active, std::memory_order_relaxed);
		}

		void OnPageOpened()
		{
			{
				std::scoped_lock lock(g_queueLock);
				g_queue.clear();
			}
			// Requested, not performed: this may be called from either thread, and the held set
			// belongs to the input thread. ObserveOutcome clears it on the next event.
			g_resetHeldPending.store(true, std::memory_order_release);
			g_cursorPrimed = false;  // centre on the first drain, at the current resolution
		}

		void OnPageClosed()
		{
			{
				std::scoped_lock lock(g_queueLock);
				g_queue.clear();
			}
			g_resetHeldPending.store(true, std::memory_order_release);
		}

		void GetCursor(float& a_x, float& a_y)
		{
			a_x = g_cursorX;
			a_y = g_cursorY;
		}

		// ---- keymap capture --------------------------------------------------------------
		//
		// The behaviours below are not invented - they are the ones Controls::HandleRebindInput
		// already implements for its five Ammo Wheel targets, and each exists because its absence
		// is a bug:
		//
		//   * a GRACE PERIOD, so the click that armed capture is not itself captured;
		//   * DOWN-EDGE ONLY (applied by the caller, which holds the ButtonEvent), so the release of
		//     the key just bound cannot overwrite it. Controls::PollRebindInput reaches the same end
		//     by polling for all-keys-up; this path is event-driven and tests the edge directly.
		//   * 0 and kInvalid rejected - an unbindable code must not become a binding;
		//   * CANCEL on Escape OR Home, both, matching Controls::IsCancelMkbKey - not Escape alone.
		//
		// Mouse-wheel codes are accepted: the engine delivers the wheel as buttons 8/9, which
		// become KEY_MOUSE_WHEEL_UP/DOWN once offset, and Wheeler binds those like any other key.

		bool BeginCaptureFromInputThread(std::uint32_t a_dispatchCode, bool a_isCancelKey)
		{
			if (!g_captureArmed.load(std::memory_order_acquire)) {
				return false;
			}

			if (WithinCaptureGrace()) {
				return true;  // inside the grace window: consume, but do not capture
			}

			if (a_isCancelKey) {
				g_captureArmed.store(false, std::memory_order_release);
				g_captureCancelled.store(true, std::memory_order_release);
				return true;
			}

			// kInvalid is (uint32)-1; 0 means unbound. Neither may become a binding.
			if (a_dispatchCode == 0u || a_dispatchCode == static_cast<std::uint32_t>(-1)) {
				return true;
			}

			g_capturedKey.store(a_dispatchCode, std::memory_order_release);
			g_captureArmed.store(false, std::memory_order_release);
			return true;
		}

		void BeginKeymapCapture()
		{
			g_capturedKey.store(0u, std::memory_order_release);
			g_captureCancelled.store(false, std::memory_order_release);
			// Long enough that the press which armed this cannot be the one captured.
			SetCaptureGrace(std::chrono::milliseconds(250));
			g_captureArmed.store(true, std::memory_order_release);
		}

		void CancelKeymapCapture()
		{
			g_captureArmed.store(false, std::memory_order_release);
			g_capturedKey.store(0u, std::memory_order_release);
			g_captureCancelled.store(false, std::memory_order_release);
		}

		bool IsCapturingKeymap()
		{
			return g_captureArmed.load(std::memory_order_acquire);
		}

		std::uint32_t TakeCapturedKey()
		{
			return g_capturedKey.exchange(0u, std::memory_order_acq_rel);
		}

		bool TakeCaptureCancelled()
		{
			return g_captureCancelled.exchange(false, std::memory_order_acq_rel);
		}

		bool InjectCapturedKey(std::uint32_t a_dispatchCode, bool a_isCancelKey)
		{
			// Collapse the grace window first. It guards against the arming CLICK being captured,
			// which a driven injection does not have - and leaving it armed would make every
			// inject-immediately-after-arm silently do nothing, which is the one behaviour that
			// would make this entry point useless for driving a test.
			SetCaptureGrace(std::chrono::milliseconds(0));
			return BeginCaptureFromInputThread(a_dispatchCode, a_isCancelKey);
		}
	}
}
