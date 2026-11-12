#include "bin/DevBench/InputInject.h"
#include "bin/DevBench/ContextStackRead.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace InputInject
{
	namespace
	{
		struct Press
		{
			RE::INPUT_DEVICE device;
			std::uint32_t code;
			int framesLeft;
			bool downSent;
			bool replay;
		};
		std::mutex g_lock;
		std::vector<Press> g_queue;
		std::vector<const RE::InputEvent*> g_replayEvents;   // spliced this dispatch as replays
		// 1.7.1: Service runs on EVERY input dispatch. With nothing queued and no replay pointers left from the last
		// dispatch it returns before taking the lock or touching anything (both written under g_lock).
		std::atomic<std::uint32_t> g_queued{ 0 };
		std::atomic<std::uint32_t> g_replaysHeld{ 0 };

		std::size_t StackOffset()
		{
			static const std::size_t offset = [] {
				const auto v = REL::Module::get().version();
				return ContextStack::OffsetFor(REL::Module::IsVR(), v[0], v[1], v[2]);
			}();
			return offset;
		}

		void Splice(RE::InputEvent** a_events, RE::INPUT_DEVICE a_device, std::uint32_t a_code, float a_value, float a_held, bool a_replay = false)
		{
			// The user event is looked up in the context the game is IN (the top of the control map's
			// context stack), not the gameplay context: inside the inventory a D-pad press must arrive
			// as the menu's own "Down", or the list never scrolls (the owner's 2026-09-13 test).
			auto* controlMap = RE::ControlMap::GetSingleton();
			// Walk the stack from the top down, the way the engine resolves a press: the inventory's
			// own context maps no D-pad name and defers to the menu-mode context beneath it (measured
			// 2026-09-13: the top context alone answered '').
			std::string_view name{};
			if (controlMap) {
				// 1.7.1: never the member. This repo's CommonLibSSE-NG checkout has no GetRuntimeData() on ControlMap and
				// lays the stack at SE's +0x100; on Skyrim 1.6.1130+ it is at +0x108, and the direct read crashed a
				// player's AE 1.6.1170 game on the first replayed D-pad tap (crash-2026-11-12-04-45-32.log,
				// InputInject.cpp:42 - the old guard checked the depth, never the pointer it then indexed).
				// ContextStackRead.h picks the offset from the running game and refuses an array that does not look like
				// one; a refusal falls through to the plain gameplay lookup below.
				const auto stack = ContextStack::Read(controlMap, StackOffset(),
					static_cast<std::uint32_t>(RE::UserEvents::INPUT_CONTEXT_ID::kTotal));
				if (stack.refused) {
					static std::atomic_bool warned{ false };
					if (!warned.exchange(true)) {
						logger::warn("[InputInject] input-context stack not read at ControlMap+0x{:X} ({}); using the gameplay context", StackOffset(), stack.refused);
					}
				}
				for (std::uint32_t i = stack.count; i > 0 && name.empty(); --i) {
					name = controlMap->GetUserEventName(a_code, a_device, static_cast<RE::UserEvents::INPUT_CONTEXT_ID>(stack.ids[i - 1]));
				}
				if (name.empty()) {
					name = controlMap->GetUserEventName(a_code, a_device);
				}
			}
			RE::BSFixedString userEvent(name.empty() ? "" : std::string(name).c_str());
			auto* ev = RE::ButtonEvent::Create(a_device, userEvent, a_code, a_value, a_held);
			if (!ev) {
				logger::warn("[InputInject] ButtonEvent::Create failed");
				return;
			}
			ev->next = *a_events;
			*a_events = ev;
			if (a_replay) {
				g_replayEvents.push_back(ev);
			}
			logger::info("[InputInject] spliced {} {} code {} userEvent '{}' held {:.2f}{}",
				a_device == RE::INPUT_DEVICE::kGamepad ? "gamepad" : (a_device == RE::INPUT_DEVICE::kMouse ? "mouse" : "keyboard"),
				a_value > 0.0f ? "DOWN" : "UP", a_code, name, a_held, a_replay ? " (replay)" : "");
		}
	}

	void QueuePress(std::uint32_t a_device, std::uint32_t a_code, int a_holdFrames, bool a_replay)
	{
		RE::INPUT_DEVICE dev = RE::INPUT_DEVICE::kKeyboard;
		if (a_device == 1) {
			dev = RE::INPUT_DEVICE::kMouse;
		} else if (a_device == 2) {
			dev = RE::INPUT_DEVICE::kGamepad;
		}
		std::scoped_lock l(g_lock);
		g_queue.push_back({ dev, a_code, a_holdFrames < 1 ? 1 : (a_holdFrames > 600 ? 600 : a_holdFrames), false, a_replay });
		g_queued.store(static_cast<std::uint32_t>(g_queue.size()));
	}

	void Service(RE::InputEvent** a_events)
	{
		if (!a_events) {
			return;
		}
		if (g_queued.load() == 0 && g_replaysHeld.load() == 0) {
			return;   // nothing queued (no DevBench inject, no D-pad tap to replay) and no stale replay pointers
		}
		std::scoped_lock l(g_lock);
		g_replayEvents.clear();   // last dispatch's pointers are dead - never let one match another mod's new event
		g_replaysHeld.store(0);
		if (g_queue.empty()) {
			g_queued.store(0);
			return;
		}
		for (auto it = g_queue.begin(); it != g_queue.end();) {
			if (!it->downSent) {
				Splice(a_events, it->device, it->code, 1.0f, 0.0f, it->replay);
				it->downSent = true;
				++it;
				continue;
			}
			if (it->framesLeft-- > 0) {
				Splice(a_events, it->device, it->code, 1.0f, 0.05f * static_cast<float>(it->framesLeft + 1), it->replay);
				++it;
				continue;
			}
			Splice(a_events, it->device, it->code, 0.0f, 0.1f, it->replay);
			it = g_queue.erase(it);
		}
		g_queued.store(static_cast<std::uint32_t>(g_queue.size()));
		g_replaysHeld.store(static_cast<std::uint32_t>(g_replayEvents.size()));
	}

	bool IsReplay(const RE::InputEvent* a_event)
	{
		if (!a_event || g_replaysHeld.load() == 0) {
			return false;   // pointer compare only, never a read through a_event: a foreign event is never taken for ours
		}
		std::scoped_lock l(g_lock);
		for (const RE::InputEvent* e : g_replayEvents) {
			if (e == a_event) {
				return true;
			}
		}
		return false;
	}

	std::uint32_t Pending()
	{
		std::scoped_lock l(g_lock);
		return static_cast<std::uint32_t>(g_queue.size());
	}
}
