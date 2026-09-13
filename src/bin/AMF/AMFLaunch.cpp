#include "bin/AMF/AMFLaunch.h"

#include "bin/Config.h"
#include "bin/SettingsPage/Page.h"
#include "bin/Texts.h"

#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <mutex>
#include <vector>

namespace AMFLaunch
{
	namespace
	{
		// ---- the AMF boundary: exported C functions, resolved by name -------------------------
		//
		// Module names, in order of preference (the same order every consumer in this project
		// uses): AMF's real module name - it is loaded as "!ApocryphaMenuFramework.dll" so it sorts
		// first in SKSE's load order - then the un-prefixed spelling, then stock SKSE Menu Framework
		// for a player who runs that instead. The SKSEMenuFramework.dll VFS alias AMF's MO2 plugin
		// creates is refused at its own SKSEPlugin_Load and unloaded, so it never answers here.
		HMODULE ResolveModule(const wchar_t*& a_name)
		{
			static const wchar_t* kNames[] = { L"!ApocryphaMenuFramework", L"ApocryphaMenuFramework", L"SKSEMenuFramework" };
			for (const wchar_t* name : kNames) {
				if (HMODULE m = GetModuleHandleW(name)) {
					a_name = name;
					return m;
				}
			}
			a_name = nullptr;
			return nullptr;
		}

		// Types mirror AMF's exports (source/main.cpp and source/Compat.cpp of the framework):
		//   bool AMF_RegisterPage(const char* mod, const char* page, void(*render)())
		//   bool IsAnyBlockingWindowOpened()
		//   void igTextWrappedV(const char* fmt, va_list)      (cimgui - varargs resolve to V)
		//   bool igButton(const char* label, ImVec2 size)       (ImVec2 = two floats, by value)
		//   void igSpacing()
		struct ImVec2Abi
		{
			float x, y;
		};
		using RegisterPageFn = bool (*)(const char*, const char*, void (*)());
		using BoolFn = bool (*)();
		using TextWrappedVFn = void (*)(const char*, va_list);
		using ButtonFn = bool (*)(const char*, ImVec2Abi);
		using VoidFn = void (*)();
		using RectOutFn = void (*)(ImVec2Abi*);   // cimgui: igGetItemRectMin/Max(ImVec2* pOut)

		struct Api
		{
			RegisterPageFn registerPage = nullptr;
			BoolFn anyBlockingWindowOpen = nullptr;
			TextWrappedVFn textWrappedV = nullptr;
			ButtonFn button = nullptr;
			VoidFn spacing = nullptr;
			RectOutFn itemRectMin = nullptr;   // optional - only the driving tool needs these
			RectOutFn itemRectMax = nullptr;

			bool Complete() const
			{
				return registerPage && anyBlockingWindowOpen && textWrappedV && button && spacing;
			}
		};

		Api g_api{};
		using ReservedKeysFn = std::uint32_t (*)(std::int32_t*, std::uint32_t);   // SMF_GetReservedKeyCodes
		std::mutex g_reservedLock;
		std::vector<std::int32_t> g_reserved;   // keyboard DIK codes the framework reported last refresh
		const char* g_registeredWith = nullptr;
		std::atomic<bool> g_pending{ false };
		std::atomic<float> g_rect[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

		template <class T>
		T Resolve(HMODULE a_module, const char* a_name, const char*& a_missing)
		{
			auto p = reinterpret_cast<T>(GetProcAddress(a_module, a_name));
			if (!p && !a_missing) {
				a_missing = a_name;
			}
			return p;
		}

		void TextWrapped(const char* a_fmt, ...)
		{
			va_list args;
			va_start(args, a_fmt);
			g_api.textWrappedV(a_fmt, args);
			va_end(args);
		}

		// ---- what AMF draws for us, inside ITS context ------------------------------------------
		// Called by AMF each frame while the player has this mod's entry selected in the Mod Control
		// Panel. Only AMF's own exports are used here; nothing from Wheeler's ImGui context.
		void RenderInsideAmf()
		{
			TextWrapped("%s", Texts::GetText(Texts::TextType::AmfLaunchExplain));
			g_api.spacing();
			if (g_api.button(Texts::GetText(Texts::TextType::AmfLaunchButton), ImVec2Abi{ 0.0f, 0.0f })) {
				ArmLaunch();
			}
			if (g_api.itemRectMin && g_api.itemRectMax) {
				ImVec2Abi mn{}, mx{};
				g_api.itemRectMin(&mn);
				g_api.itemRectMax(&mx);
				g_rect[0].store(mn.x); g_rect[1].store(mn.y); g_rect[2].store(mx.x); g_rect[3].store(mx.y);
			}
			if (g_pending.load()) {
				g_api.spacing();
				TextWrapped("%s", Texts::GetText(Texts::TextType::AmfLaunchPending));
			}
		}
	}

	void Register()
	{
		if (g_registeredWith) {
			return;
		}

		const wchar_t* moduleName = nullptr;
		HMODULE module = ResolveModule(moduleName);
		if (!module) {
			logger::info("[AMFLaunch] no menu framework module is loaded; the settings page is reachable by its own key only");
			return;
		}

		const char* missing = nullptr;
		Api api{};
		api.registerPage = Resolve<RegisterPageFn>(module, "AMF_RegisterPage", missing);
		api.anyBlockingWindowOpen = Resolve<BoolFn>(module, "IsAnyBlockingWindowOpened", missing);
		api.textWrappedV = Resolve<TextWrappedVFn>(module, "igTextWrappedV", missing);
		api.button = Resolve<ButtonFn>(module, "igButton", missing);
		api.spacing = Resolve<VoidFn>(module, "igSpacing", missing);
		const char* optionalMissing = nullptr;   // these two are not required to register
		api.itemRectMin = Resolve<RectOutFn>(module, "igGetItemRectMin", optionalMissing);
		api.itemRectMax = Resolve<RectOutFn>(module, "igGetItemRectMax", optionalMissing);

		if (!api.Complete()) {
			// Fail closed: registering a callback that would call a null pointer is a crash on the
			// first frame the entry is selected. Stock SKSE Menu Framework lacks AMF_RegisterPage,
			// so this is the branch a stock-SMF player lands in; their page still opens by its key.
			logger::warn("[AMFLaunch] framework module found but export '{}' is missing - not registering (the page stays reachable by its key)",
				missing ? missing : "?");
			return;
		}

		g_api = api;
		const bool ok = g_api.registerPage("Wheeler - Refined", "Settings", &RenderInsideAmf);
		if (!ok) {
			logger::warn("[AMFLaunch] AMF_RegisterPage refused the registration (see the framework's log)");
			return;
		}

		static char s_name[64] = {};
		WideCharToMultiByte(CP_UTF8, 0, moduleName, -1, s_name, sizeof(s_name), nullptr, nullptr);
		g_registeredWith = s_name;
		logger::info("[AMFLaunch] registered 'Wheeler - Refined / Settings' with {}", g_registeredWith);
	}

	void Tick()
	{
		if (!g_pending.load()) {
			return;
		}
		if (g_api.anyBlockingWindowOpen && g_api.anyBlockingWindowOpen()) {
			return;  // AMF's menu is still up; opening now would put two menus on the same input
		}
		g_pending.store(false);
		SettingsPage::Page::SetOpen(true);
		logger::info("[AMFLaunch] framework menu closed; settings page opened");
	}

	bool IsLaunchPending()
	{
		return g_pending.load();
	}

	void ArmLaunch()
	{
		g_pending.store(true);
		logger::info("[AMFLaunch] launch armed; the page opens when the framework's menu closes");
	}

	Rect GetButtonRect()
	{
		return Rect{ g_rect[0].load(), g_rect[1].load(), g_rect[2].load(), g_rect[3].load() };
	}

	const char* RegisteredWith()
	{
		return g_registeredWith ? g_registeredWith : "";
	}

	void RefreshReservedKeys()
	{
		std::vector<std::int32_t> fresh;
		const wchar_t* moduleName = nullptr;
		if (HMODULE module = ResolveModule(moduleName)) {
			// Resolved on every refresh rather than cached: the answer must follow the module that is
			// loaded NOW, and a cached null from an early call would hide a framework that loaded later.
			const auto func = reinterpret_cast<ReservedKeysFn>(GetProcAddress(module, "SMF_GetReservedKeyCodes"));
			if (func) {
				const std::uint32_t needed = func(nullptr, 0);   // null buffer = "how many" (the DEM probe)
				if (needed > 0 && needed < 256) {
					fresh.resize(needed);
					const std::uint32_t written = func(fresh.data(), needed);
					fresh.resize(written < needed ? written : needed);
				}
			}
		}
		std::lock_guard lock(g_reservedLock);
		g_reserved.swap(fresh);
	}

	bool IsKeyReservedByFramework(std::uint32_t a_code)
	{
		if (a_code == 0u || a_code >= 256u) {
			return false;   // unbound, mouse (+256) or gamepad (+266): the framework reserves keyboard keys only
		}
		std::lock_guard lock(g_reservedLock);
		for (const std::int32_t code : g_reserved) {
			if (code >= 0 && static_cast<std::uint32_t>(code) == a_code) {
				return true;
			}
		}
		return false;
	}

	void ApplyReservedKeyPolicy()
	{
		RefreshReservedKeys();
		if (ReservedKeyCount() == 0) {
			return;
		}
		struct Opener
		{
			const char* name;
			std::uint32_t* value;
		};
		const Opener openers[] = {
			{ "Control.Wheel/SettingsPageKey", &Config::Control::Wheel::SettingsPageKey },
			{ "InputBindings.MKB/toggleWheel", &Config::InputBindings::MKB::toggleWheel },
			{ "InputBindings.MKB/toggleWheelModifier", &Config::InputBindings::MKB::toggleWheelModifier },
		};
		for (const Opener& opener : openers) {
			if (IsKeyReservedByFramework(*opener.value)) {
				logger::warn("[AMFLaunch] {} = {} is a key the menu framework reserves (its menu or navigation key); left UNBOUND - rebind it on the Wheeler settings page",
					opener.name, *opener.value);
				*opener.value = 0u;
			}
		}
	}

	bool OpenFrameworkMenuOnUs()
	{
		const wchar_t* moduleName = nullptr;
		HMODULE module = ResolveModule(moduleName);
		if (!module) {
			return false;
		}
		using OpenFn = bool (*)(const char*);
		const auto open = reinterpret_cast<OpenFn>(GetProcAddress(module, "AMF_OpenMenu"));
		if (!open) {
			logger::info("[AMFLaunch] the loaded framework has no AMF_OpenMenu export (needs Apocrypha Menu Framework 1.7.7)");
			return false;
		}
		open("Wheeler - Refined");
		return true;
	}

	std::uint32_t FrameworkMenuKey()
	{
		std::lock_guard lock(g_reservedLock);
		return g_reserved.empty() || g_reserved.front() <= 0 ? 0u : static_cast<std::uint32_t>(g_reserved.front());
	}

	std::size_t ReservedKeyCount()
	{
		std::lock_guard lock(g_reservedLock);
		return g_reserved.size();
	}
}
