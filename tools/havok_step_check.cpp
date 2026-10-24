// Off-game check of src/bin/Utilities/HavokStepCore.h - Havok's step follows the wheel's slow time (1.5.0, the flying
// corpses iSlyy0 reported on Nexus, 2026-09-19).
//   tools\havok_step_check.bat
// Prints one line per check and "havok_step_check: N of N passed"; exits 1 on any failure.
#include "../src/bin/Utilities/HavokStepCore.h"

#include <cmath>
#include <iostream>
#include <sstream>
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

	// SSE Display Tweaks' DynamicMaxTimeScaling (ac90): a per-frame writer owns fMaxTime, the gate stands down.
	{
		Gate g;
		g.StandDown();
		Check(!g.OnTimeScale(0.2f, njord) && !g.Active(), "standing down (Display Tweaks found): the wheel opening writes nothing");
		Check(!g.OnTimeScale(1.0f, njord) && !g.LastCloseLost(), "... and its close writes nothing and is not counted as lost");
	}
	{
		Gate g;
		g.OnTimeScale(0.2f, njord);
		g.StandDown();   // decided while a write of ours is in place
		auto v = g.OnTimeScale(1.0f, njord * 0.2f);
		Check(v && Near(*v, njord), "standing down with our value in place: the close still puts the original back");
		Check(!g.OnTimeScale(0.2f, njord), "... and the next opening writes nothing");
	}
	{
		Gate g;
		const float dt[] = { 0.0166f, 0.0171f, 0.0169f };   // a per-frame writer: a different value at every close
		for (int i = 0; i < kLostInRow; ++i)
		{
			g.OnTimeScale(0.2f, dt[i]);
			g.OnTimeScale(1.0f, dt[i] + 0.001f);
			Check(g.LastCloseLost(), "close " + std::to_string(i + 1) + " found another mod's value (LastCloseLost)");
			Check(g.StandingDown() == (i + 1 >= kLostInRow), "... standing down only from close " + std::to_string(kLostInRow));
		}
		Check(!g.OnTimeScale(0.2f, 0.0168f), "after kLostInRow lost closes the next opening writes nothing");
	}
	{
		Gate g;
		g.OnTimeScale(0.2f, njord);
		g.OnTimeScale(1.0f, 0.016f);   // Havok Fix, once
		g.OnTimeScale(0.2f, 0.016f);
		auto v = g.OnTimeScale(1.0f, 0.016f * 0.2f);
		Check(v && !g.StandingDown(), "an occasional writer (one lost close, then ours again) is still followed");
		g.OnTimeScale(0.2f, 0.016f);
		g.OnTimeScale(1.0f, 0.02f);
		g.OnTimeScale(0.2f, 0.02f);
		g.OnTimeScale(1.0f, 0.021f);
		Check(!g.StandingDown(), "... a successful close resets the run (two lost after it do not stand down)");
	}
	{
		const auto read = [](const char* a_text) { std::istringstream in(a_text); return ReadDynamicMaxTimeScaling(in); };
		Check(read("[HAVOK]\nDynamicMaxTimeScaling = true\n") == true, "Display Tweaks INI: [HAVOK] DynamicMaxTimeScaling = true");
		Check(read("[Havok]\r\ndynamicmaxtimescaling=0 ; off\r\n") == false, "... case, CRLF, '0' and a trailing comment");
		Check(!read("[HAVOK]\n# DynamicMaxTimeScaling = true\n"), "... a commented-out line sets nothing");
		Check(!read("[Render]\nDynamicMaxTimeScaling = true\n"), "... the key in another section is not it");
		Check(read("[HAVOK]\nDynamicMaxTimeScaling = true\n[HAVOK]\nDynamicMaxTimeScaling = false\n") == false,
			"... the last value wins");
	}

	std::cout << "havok_step_check: " << g_pass << " of " << (g_pass + g_fail) << " passed\n";
	return g_fail ? 1 : 0;
}
