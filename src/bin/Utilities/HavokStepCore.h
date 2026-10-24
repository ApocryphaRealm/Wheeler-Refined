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

#include <algorithm>
#include <cmath>
#include <optional>

namespace havokstep
{
	inline constexpr float kMinStep = 0.0005f;   // 2000 Hz: never ask Havok for a smaller step than this
	inline constexpr float kNormal = 0.999f;     // a time scale at or above this is normal time
	inline constexpr float kSame = 1e-6f;

	class Gate
	{
	public:
		// The global time multiplier is about to be set to a_scale; a_current is fMaxTime:HAVOK now. Returns the value to
		// write into fMaxTime, or nullopt to leave it as it is.
		std::optional<float> OnTimeScale(float a_scale, float a_current)
		{
			if (!(a_current > 0.0f)) { return std::nullopt; }   // no setting to scale
			if (a_scale > 0.0f && a_scale < kNormal)
			{
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
			if (std::fabs(a_current - m_applied) > kSame) { return std::nullopt; }   // someone else owns it now
			return m_base;
		}

		bool Active() const { return m_active; }
		float Base() const { return m_base; }

	private:
		bool  m_active = false;
		float m_base = 0.0f;
		float m_applied = 0.0f;
	};
}
