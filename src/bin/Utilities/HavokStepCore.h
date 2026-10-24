#pragma once

// HAVOK'S STEP FOLLOWS THE WHEEL'S SLOW TIME (1.5.0) - pure logic, no game types, so tools\havok_step_check.cpp tests it.
//
// iSlyy0 on Nexus, 2026-09-19: "corpses go haywire when I open the wheel near them. They fly everywhere like crazy."
// Opening the wheel slows time (SlowTimeScale, 0.5 shipped, 0.2 for many players, the ammo wheel 0.2) through the global
// time multiplier, so the game delta Havok steps with is a fifth of a frame - while fMaxTime:HAVOK, the step size Havok
// is tuned for, stays where it was set for real time (Njordlinger pins 1/120 s). A ragdoll in contact with the ground and
// with other bodies resolves its penetration as a velocity per unit of time, so a step far smaller than the tuning gives
// a far larger separating velocity, and when time is restored in one write the bodies fly. Havok Fix (Nexus 18160)
// retunes fMaxTime from the live frame rate for the same reason. So while time is slowed, fMaxTime is scaled by the same
// factor (the step stays the same size in REAL time), and it is put back when time runs normally again.
//
// Another mod may also write fMaxTime (Havok Fix does, every so often). The value is only put back when it is still the
// one this wrote; a value changed by someone else meanwhile becomes the new base when time is slowed again, and is left
// alone on the way back.
//
// A mod that rewrites it EVERY FRAME owns it outright (combined run 2026-10-24, test ac90: corpses still, but no restore
// line on close - SSE Display Tweaks' [HAVOK] DynamicMaxTimeScaling sets fMaxTime from the frame time each frame, so the
// value at the close was never this one's and the restore was skipped in silence). Writing under it is a fight that one
// frame later is lost anyway. So the gate STANDS DOWN - writes nothing more - when Display Tweaks is loaded with that
// option on (ReadDynamicMaxTimeScaling, read once at the first slow-down), or after kLostInRow closes in a row found the
// value changed by someone else (any other per-frame writer). An occasional writer such as Havok Fix loses one close now
// and then and is still followed.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <istream>
#include <optional>
#include <string>

namespace havokstep
{
	inline constexpr float kMinStep = 0.0005f;   // 2000 Hz: never ask Havok for a smaller step than this
	inline constexpr float kNormal = 0.999f;     // a time scale at or above this is normal time
	inline constexpr float kSame = 1e-6f;
	inline constexpr int kLostInRow = 3;         // closes in a row lost to another writer before the gate stands down

	// SSEDisplayTweaks.ini (or its _custom.ini): the [HAVOK] section's DynamicMaxTimeScaling, nullopt when the file does
	// not set it. '#' and ';' start comments; section and key names are matched without regard to case.
	inline std::optional<bool> ReadDynamicMaxTimeScaling(std::istream& a_in)
	{
		const auto lower = [](std::string s) {
			for (auto& c : s) { c = static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
			return s;
		};
		const auto trim = [](std::string s) {
			const auto b = s.find_first_not_of(" \t\r");
			const auto e = s.find_last_not_of(" \t\r");
			return b == std::string::npos ? std::string() : s.substr(b, e - b + 1);
		};
		std::optional<bool> found;
		bool inHavok = false;
		for (std::string line; std::getline(a_in, line);)
		{
			line = trim(line.substr(0, line.find_first_of("#;")));
			if (line.empty()) { continue; }
			if (line.front() == '[')
			{
				inHavok = lower(line) == "[havok]";
				continue;
			}
			const auto eq = line.find('=');
			if (!inHavok || eq == std::string::npos || lower(trim(line.substr(0, eq))) != "dynamicmaxtimescaling") { continue; }
			const std::string v = lower(trim(line.substr(eq + 1)));
			if (v == "true" || v == "1") { found = true; }
			else if (v == "false" || v == "0") { found = false; }
		}
		return found;
	}

	class Gate
	{
	public:
		// The global time multiplier is about to be set to a_scale; a_current is fMaxTime:HAVOK now. Returns the value to
		// write into fMaxTime, or nullopt to leave it as it is.
		std::optional<float> OnTimeScale(float a_scale, float a_current)
		{
			m_lastCloseLost = false;
			if (!(a_current > 0.0f)) { return std::nullopt; }   // no setting to scale
			if (a_scale > 0.0f && a_scale < kNormal)
			{
				if (m_standDown && !m_active) { return std::nullopt; }   // another mod owns fMaxTime
				if (!m_active || std::fabs(a_current - m_applied) > kSame)
				{
					m_base = a_current;   // first slow-down, or someone else retuned it since our write
				}
				m_active = true;
				m_applied = (std::max)(m_base * a_scale, kMinStep);   // parenthesised: windows.h defines a max macro
				if (m_applied > m_base) { m_applied = m_base; }   // a base already under the floor is left as it is
				return m_applied;
			}
			if (!m_active) { return std::nullopt; }
			m_active = false;
			if (std::fabs(a_current - m_applied) > kSame)   // someone else owns it now
			{
				m_lastCloseLost = true;
				if (++m_lostInRow >= kLostInRow) { m_standDown = true; }
				return std::nullopt;
			}
			m_lostInRow = 0;
			return m_base;
		}

		// No more writes from the next slow-down on (a write already made is still put back at its close).
		void StandDown() { m_standDown = true; }

		bool Active() const { return m_active; }
		float Base() const { return m_base; }
		bool StandingDown() const { return m_standDown; }
		bool LastCloseLost() const { return m_lastCloseLost; }   // the last call was a close that found another mod's value

	private:
		bool  m_active = false;
		bool  m_standDown = false;
		bool  m_lastCloseLost = false;
		int   m_lostInRow = 0;
		float m_base = 0.0f;
		float m_applied = 0.0f;
	};
}
