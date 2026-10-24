// Off-game check of src/bin/Utilities/HavokStepCore.h - Havok's step follows the wheel's slow time (1.5.0, the flying
// corpses iSlyy0 reported on Nexus, 2026-09-19).
//   tools\havok_step_check.bat
// Prints one line per check and "havok_step_check: N of N passed"; exits 1 on any failure.
#include "../src/bin/Utilities/HavokStepCore.h"

#include <cmath>
#include <iostream>
#include <string>

namespace
{
	int g_pass = 0, g_fail = 0;

	void Check(bool a_ok, const std::string& a_what)
	{
		(a_ok ? g_pass : g_fail)++;
		std::cout << (a_ok ? "PASS " : "FAIL ") << a_what << "\n";
	}

	bool Near(float a, float b) { return std::fabs(a - b) < 1e-7f; }
}

int main()
{
	using namespace havokstep;
	const float njord = 0.00833333f;   // Njordlinger's fMaxTime (1/120 s)

	{
		Gate g;
		auto v = g.OnTimeScale(0.2f, njord);
		Check(v && Near(*v, njord * 0.2f), "the wheel opens at 0.2: fMaxTime is scaled by 0.2 (the step stays the same in real time)");
		Check(g.Active() && Near(g.Base(), njord), "... and the original is remembered");
		v = g.OnTimeScale(1.0f, njord * 0.2f);
		Check(v && Near(*v, njord), "the wheel closes (1.0): the original fMaxTime is put back");
		Check(!g.Active(), "... and the gate is idle again");
		Check(!g.OnTimeScale(1.0f, njord), "a normal time scale with nothing applied writes nothing");
	}
	{
		Gate g;
		g.OnTimeScale(0.5f, njord);
		auto v = g.OnTimeScale(0.2f, njord * 0.5f);   // the scale changes while open (mounted floor, the ammo wheel)
		Check(v && Near(*v, njord * 0.2f), "a second slow-down scales from the ORIGINAL, not from the scaled value");
		v = g.OnTimeScale(1.0f, njord * 0.2f);
		Check(v && Near(*v, njord), "... and the close still puts back the original");
	}
	{
		Gate g;
		g.OnTimeScale(0.2f, njord);
		auto v = g.OnTimeScale(1.0f, 0.016f);   // Havok Fix retuned fMaxTime while the wheel was open
		Check(!v && !g.Active(), "a value someone else wrote meanwhile is left alone at the close");
		v = g.OnTimeScale(0.2f, 0.016f);
		Check(v && Near(*v, 0.016f * 0.2f) && Near(g.Base(), 0.016f), "the next slow-down takes that value as the new base");
	}
	{
		Gate g;
		auto v = g.OnTimeScale(0.01f, njord);   // the floor of SlowTimeScale
		Check(v && Near(*v, kMinStep), "a very small time scale is floored at kMinStep (no step under 0.5 ms)");
		g.OnTimeScale(1.0f, kMinStep);
		v = g.OnTimeScale(0.2f, 0.0002f);   // a base already under the floor
		Check(v && Near(*v, 0.0002f), "a base already under the floor is not raised");
	}
	{
		Gate g;
		Check(!g.OnTimeScale(0.2f, 0.0f), "no fMaxTime setting (0): nothing is written");
		Check(!g.OnTimeScale(0.0f, njord) && !g.Active(), "a scale of 0 (the pause path never calls this) writes nothing");
	}

	std::cout << "havok_step_check: " << g_pass << " of " << (g_pass + g_fail) << " passed\n";
	return g_fail ? 1 : 0;
}
