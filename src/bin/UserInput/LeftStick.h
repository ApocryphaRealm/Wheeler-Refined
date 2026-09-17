#pragma once

// LEFT STICK AS A WHEEL CONTROL (1.3.1; the owner, 2026-09-17: "a toggle that prevents player movement
// mapping to the left stick while in the wheel menu" ... "The whole point of dropping left stick movement
// is so that we can map a control to it for the wheel menu" ... "we can have the move wheel forward set
// to the left stick").
//
// While the wheel is open and Config::Control::Wheel::LeftStickWheelControl is on, the left thumbstick's
// events are consumed (the character stops moving) and its four directions act as gamepad BUTTONS the
// wheel's actions bind to: codes 282..285 = Left Stick Up / Down / Left / Right (266 + 16..19, after the
// sixteen real buttons). A flick past 0.6 presses the direction; returning inside 0.3 releases it. Move
// Wheel Forward ships on Left Stick Right and Move Wheel Back on Left Stick Left.
#include <cstdint>

namespace LeftStick
{
	constexpr std::uint32_t kGamepadOffset = 266;
	constexpr std::uint32_t kUp = kGamepadOffset + 16;
	constexpr std::uint32_t kDown = kGamepadOffset + 17;
	constexpr std::uint32_t kLeft = kGamepadOffset + 18;
	constexpr std::uint32_t kRight = kGamepadOffset + 19;
	constexpr float kPressAt = 0.6f;
	constexpr float kReleaseAt = 0.3f;

	inline bool IsDirectionCode(std::uint32_t a_code) { return a_code >= kUp && a_code <= kRight; }
	// The direction a stick position points to as a code, or 0 inside the press threshold.
	std::uint32_t CodeFor(float a_x, float a_y);

	// Feed one left-stick sample while the wheel is open. Dispatches the press and release edges through
	// Controls::Dispatch like a real button. Returns the code pressed or released this call (0 if none).
	std::uint32_t Feed(float a_x, float a_y);
	// Releases whatever direction is held (wheel closed, toggle switched off).
	void Reset();
	std::uint32_t Held();
}
