#include "bin/UserInput/LeftStick.h"

#include "bin/UserInput/Controls.h"

#include <cmath>

namespace LeftStick
{
	namespace
	{
		std::uint32_t g_held = 0;
	}

	std::uint32_t CodeFor(float a_x, float a_y)
	{
		const float ax = std::fabs(a_x), ay = std::fabs(a_y);
		if (ax < kPressAt && ay < kPressAt) { return 0; }
		if (ax >= ay) { return a_x > 0.0f ? kRight : kLeft; }
		return a_y > 0.0f ? kUp : kDown;   // the engine reports up as positive y
	}

	std::uint32_t Feed(float a_x, float a_y)
	{
		const float mag = (std::max)(std::fabs(a_x), std::fabs(a_y));
		if (g_held != 0) {
			if (mag < kReleaseAt) {
				const std::uint32_t released = g_held;
				g_held = 0;
				Controls::Dispatch(released, false, true);
				return released;
			}
			return 0;   // still held: no repeat, no re-aim until the stick comes back
		}
		const std::uint32_t code = CodeFor(a_x, a_y);
		if (code == 0) { return 0; }
		g_held = code;
		Controls::Dispatch(code, true, true);
		return code;
	}

	void Reset()
	{
		if (g_held != 0) {
			const std::uint32_t released = g_held;
			g_held = 0;
			Controls::Dispatch(released, false, true);
		}
	}

	std::uint32_t Held() { return g_held; }
}
