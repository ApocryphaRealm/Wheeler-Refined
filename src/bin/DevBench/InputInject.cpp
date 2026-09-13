#include "bin/DevBench/InputInject.h"

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
		};
		std::mutex g_lock;
		std::vector<Press> g_queue;

		void Splice(RE::InputEvent** a_events, RE::INPUT_DEVICE a_device, std::uint32_t a_code, float a_value, float a_held)
		{
			auto* controlMap = RE::ControlMap::GetSingleton();
			const std::string_view name = controlMap ? controlMap->GetUserEventName(a_code, a_device) : std::string_view{};
			RE::BSFixedString userEvent(name.empty() ? "" : std::string(name).c_str());
			auto* ev = RE::ButtonEvent::Create(a_device, userEvent, a_code, a_value, a_held);
			if (!ev) {
				logger::warn("[InputInject] ButtonEvent::Create failed");
				return;
			}
			ev->next = *a_events;
			*a_events = ev;
			logger::info("[InputInject] spliced {} {} code {} userEvent '{}' held {:.2f}",
				a_device == RE::INPUT_DEVICE::kGamepad ? "gamepad" : (a_device == RE::INPUT_DEVICE::kMouse ? "mouse" : "keyboard"),
				a_value > 0.0f ? "DOWN" : "UP", a_code, name, a_held);
		}
	}

	void QueuePress(std::uint32_t a_device, std::uint32_t a_code, int a_holdFrames)
	{
		RE::INPUT_DEVICE dev = RE::INPUT_DEVICE::kKeyboard;
		if (a_device == 1) {
			dev = RE::INPUT_DEVICE::kMouse;
		} else if (a_device == 2) {
			dev = RE::INPUT_DEVICE::kGamepad;
		}
		std::scoped_lock l(g_lock);
		g_queue.push_back({ dev, a_code, a_holdFrames < 1 ? 1 : (a_holdFrames > 600 ? 600 : a_holdFrames), false });
	}

	void Service(RE::InputEvent** a_events)
	{
		if (!a_events) {
			return;
		}
		std::scoped_lock l(g_lock);
		if (g_queue.empty()) {
			return;
		}
		for (auto it = g_queue.begin(); it != g_queue.end();) {
			if (!it->downSent) {
				Splice(a_events, it->device, it->code, 1.0f, 0.0f);
				it->downSent = true;
				++it;
				continue;
			}
			if (it->framesLeft-- > 0) {
				Splice(a_events, it->device, it->code, 1.0f, 0.05f * static_cast<float>(it->framesLeft + 1));
				++it;
				continue;
			}
			Splice(a_events, it->device, it->code, 0.0f, 0.1f);
			it = g_queue.erase(it);
		}
	}

	std::uint32_t Pending()
	{
		std::scoped_lock l(g_lock);
		return static_cast<std::uint32_t>(g_queue.size());
	}
}
