#include "RenderManager.h"

#include "bin/AMF/AMFLaunch.h"
#include <d3d11.h>

#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

#include <dxgi.h>
#include <sstream>
#include <algorithm>
#include <cctype>

#include "imgui_internal.h"
// Renderer hook/setup patterns derive from LamasTinyHUD revision
// dd1794c46b1f87cbf04a5d60968facbed0605d02 (GNU GPL v3), inherited through
// original Wheeler. The ImGui FreeType integration also follows
// MaxsuDetectionMeter revision fcc5ef75d6cdabc63db0b214bc61fc272a5b22cf
// (MIT).
#include "include/lib/imgui_freetype.h"

#include "bin/Wheeler/Wheeler.h"
#include "bin/Wheeler/AmmoWheelReskin.h"
#include "bin/Wheeler/AmmoWheelReskinUnified.h"
#include "bin/Rendering/TextureManager.h"
#include "bin/Rendering/ResolutionScaleContext.h"
#include "bin/InitState.h"
#include "bin/Config.h"
#include "bin/Texts.h"
#include <mutex>
#include <vector>
#include "bin/SettingsPage/Page.h"
#include "bin/SettingsPage/PageInput.h"

#include "bin/Animation/TimeInterpolator/TimeInterpolatorManager.h"

// ========== GLYPH RANGE BUILDING ==========
namespace GlyphRanges
{
	// Static glyph range arrays (must persist for ImGui font building)
	// Preset 0: Minimal (ASCII only)
	static const ImWchar RangesMinimal[] = {
		0x0020, 0x007F,  // Basic Latin (ASCII printable)
		0
	};

	// Preset 1: Latin Basic (ASCII + Latin-1 Supplement)
	static const ImWchar RangesLatinBasic[] = {
		0x0020, 0x007F,  // Basic Latin
		0x00A0, 0x00FF,  // Latin-1 Supplement (German, French, Spanish, etc.)
		0
	};

	// Preset 2: Latin Extended (covers most European languages)
	static const ImWchar RangesLatinExtended[] = {
		0x0020, 0x007F,  // Basic Latin
		0x00A0, 0x00FF,  // Latin-1 Supplement
		0x0100, 0x017F,  // Latin Extended-A (Central European: Polish, Czech, Hungarian, Turkish, etc.)
		0x0180, 0x024F,  // Latin Extended-B (Romanian, Croatian, Slovenian, etc.)
		0
	};

	// Preset 3: Latin Full (comprehensive European + phonetic)
	static const ImWchar RangesLatinFull[] = {
		0x0020, 0x007F,  // Basic Latin
		0x00A0, 0x00FF,  // Latin-1 Supplement
		0x0100, 0x017F,  // Latin Extended-A
		0x0180, 0x024F,  // Latin Extended-B
		0x0250, 0x02AF,  // IPA Extensions (phonetic symbols)
		0x02B0, 0x02FF,  // Spacing Modifier Letters
		0x1E00, 0x1EFF,  // Latin Extended Additional (Vietnamese, Welsh, etc.)
		0x2000, 0x206F,  // General Punctuation
		0x20A0, 0x20CF,  // Currency Symbols
		0
	};

	// Storage for custom parsed ranges
	static std::vector<ImWchar> CustomRangesStorage;

	// Parse custom ranges string like "0020-007E,00A0-00FF,0100-017F"
	static bool ParseCustomRanges(const std::string& rangesStr, std::vector<ImWchar>& outRanges)
	{
		outRanges.clear();
		if (rangesStr.empty()) {
			return false;
		}

		std::stringstream ss(rangesStr);
		std::string token;
		
		while (std::getline(ss, token, ',')) {
			// Trim whitespace
			token.erase(0, token.find_first_not_of(" \t"));
			token.erase(token.find_last_not_of(" \t") + 1);
			
			if (token.empty()) continue;
			
			// Parse "XXXX-YYYY" format
			size_t dashPos = token.find('-');
			if (dashPos == std::string::npos || dashPos == 0 || dashPos == token.length() - 1) {
				logger::warn("[Font] Invalid range format: '{}' - expected XXXX-YYYY", token);
				continue;
			}
			
			std::string startStr = token.substr(0, dashPos);
			std::string endStr = token.substr(dashPos + 1);
			
			try {
				unsigned long startVal = std::stoul(startStr, nullptr, 16);
				unsigned long endVal = std::stoul(endStr, nullptr, 16);
				
				// Validate range (ImWchar is typically 16-bit, so cap at 0xFFFF for BMP)
				if (startVal > 0xFFFF || endVal > 0xFFFF) {
					logger::warn("[Font] Range exceeds BMP (0xFFFF): {}-{}, clamping", startStr, endStr);
					if (startVal > 0xFFFF) startVal = 0xFFFF;
					if (endVal > 0xFFFF) endVal = 0xFFFF;
				}
				
				if (startVal > endVal) {
					logger::warn("[Font] Invalid range (start > end): {}-{}", startStr, endStr);
					continue;
				}
				
				outRanges.push_back(static_cast<ImWchar>(startVal));
				outRanges.push_back(static_cast<ImWchar>(endVal));
			}
			catch (const std::exception& e) {
				logger::warn("[Font] Failed to parse range '{}': {}", token, e.what());
				continue;
			}
		}
		
		// Terminate with 0
		if (!outRanges.empty()) {
			outRanges.push_back(0);
			return true;
		}
		return false;
	}

	// Get glyph ranges based on preset
	static const ImWchar* GetGlyphRangesForPreset(int preset, const std::string& customRanges)
	{
		switch (preset) {
		case 0:  // Minimal
			INFO("[Font] Using Minimal glyph preset (ASCII only)");
			return RangesMinimal;
			
		case 1:  // Latin Basic
			INFO("[Font] Using Latin Basic glyph preset (ASCII + Latin-1 Supplement)");
			return RangesLatinBasic;
			
		case 2:  // Latin Extended (default)
			INFO("[Font] Using Latin Extended glyph preset (Latin-1 + Extended-A + Extended-B)");
			return RangesLatinExtended;
			
		case 3:  // Latin Full
			INFO("[Font] Using Latin Full glyph preset (Extended + Additional + IPA + Currency)");
			return RangesLatinFull;
			
		case 4:  // Custom
			if (ParseCustomRanges(customRanges, CustomRangesStorage)) {
				INFO("[Font] Using Custom glyph ranges: {}", customRanges);
				return CustomRangesStorage.data();
			} else {
				logger::warn("[Font] Custom ranges parsing failed, falling back to Latin Extended");
				return RangesLatinExtended;
			}
			
		default:
			logger::warn("[Font] Unknown glyph preset {}, falling back to Latin Extended", preset);
			return RangesLatinExtended;
		}
	}

	// Get preset name for logging
	static const char* GetPresetName(int preset)
	{
		switch (preset) {
		case 0: return "Minimal";
		case 1: return "Latin Basic";
		case 2: return "Latin Extended";
		case 3: return "Latin Full";
		case 4: return "Custom";
		default: return "Unknown";
		}
	}

	// Count glyphs in a range array
	static int CountGlyphsInRanges(const ImWchar* ranges)
	{
		if (!ranges) return 0;
		int count = 0;
		for (int i = 0; ranges[i] != 0; i += 2) {
			count += (ranges[i + 1] - ranges[i] + 1);
		}
		return count;
	}

	// Log range details
	static void LogRangeDetails(const ImWchar* ranges)
	{
		if (!ranges) return;
		INFO("[Font] Glyph ranges:");
		for (int i = 0; ranges[i] != 0; i += 2) {
			INFO("[Font]   U+{:04X} - U+{:04X} ({} glyphs)", 
				(unsigned int)ranges[i], (unsigned int)ranges[i + 1], 
				(ranges[i + 1] - ranges[i] + 1));
		}
		INFO("[Font] Total potential glyphs: {}", CountGlyphsInRanges(ranges));
	}
}
// ========== END GLYPH RANGE BUILDING ==========


namespace stl
{
	using namespace SKSE::stl;

	template <class T>
	void write_thunk_call()
	{
		auto& trampoline = SKSE::GetTrampoline();
		const REL::Relocation<std::uintptr_t> hook{ T::id, T::offset };
		T::func = trampoline.write_call<5>(hook.address(), T::thunk);
	}
}

namespace
{
	bool ValidateCallHookSite(std::uintptr_t address, const char* label)
	{
		const auto opcode = *reinterpret_cast<std::uint8_t*>(address);
		if (opcode != 0xE8) {
			logger::warn("RenderManager: {} hook site {:X} has unexpected opcode {:02X}; skipping install",
				label,
				address,
				static_cast<std::uint32_t>(opcode));
			return false;
		}

		return true;
	}
}


LRESULT RenderManager::WndProcHook::thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto& io = ImGui::GetIO();
	if (uMsg == WM_KILLFOCUS) {
		io.ClearInputCharacters();
		io.ClearInputKeys();
	}

	return func(hWnd, uMsg, wParam, lParam);
}

// ========== FONT ATLAS ==========
// 1.2.7 (littlefot's report on the Nexus page, 2026-09-17): with `font = japanese` in FontConfig.ini
// the Japanese face loaded but the log read "Language 'japanese' - using GlyphPreset 3 (Latin Full)"
// and every kana and kanji drew as '?'. The folder name was compared CASE-SENSITIVELY against
// "Japanese", so a lower-case folder - which Windows opens just the same - fell through to the Latin
// presets. Three things changed, all in this one function:
//   1. the folder name is matched without regard to case;
//   2. the ranges are BUILT rather than picked: the preset the player chose (English fallback strings
//      need it), the built-in ranges of the folder's script, the built-in ranges of the GAME's language
//      (a Japanese game shows Japanese item names on the wheel whatever the folder says), and every
//      character of the loaded translation - so no table can be wrong or incomplete;
//   3. when the needed script is one the chosen face lacks, a system face that has it is MERGED in
//      (MergeMode adds only the glyphs still missing) - the same fallback the menu framework uses.
// The atlas is rebuilt once the translations are loaded (kDataLoaded, through RequestFontRebuild)
// and again whenever the language is switched, outside a frame.
namespace
{
	std::string LowerAscii(std::string a_s)
	{
		std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return a_s;
	}

	// The script a language name asks for beyond Latin. Both the game's sLanguage values
	// ("japanese", "russian") and the folder names FontConfig.ini documents ("Japanese", "Cyrillic")
	// arrive here lower-cased.
	enum class Script { None, Japanese, Korean, Chinese, Thai, Vietnamese, Cyrillic };

	Script ScriptFor(const std::string& a_lower)
	{
		if (a_lower == "japanese") { return Script::Japanese; }
		if (a_lower == "korean") { return Script::Korean; }
		if (a_lower == "chinese" || a_lower == "chinesesimplified" || a_lower == "chinesetraditional" || a_lower == "schinese" || a_lower == "tchinese") { return Script::Chinese; }
		if (a_lower == "thai") { return Script::Thai; }
		if (a_lower == "vietnamese") { return Script::Vietnamese; }
		if (a_lower == "cyrillic" || a_lower == "russian" || a_lower == "ukrainian" || a_lower == "bulgarian" || a_lower == "serbian" || a_lower == "belarusian") { return Script::Cyrillic; }
		return Script::None;
	}

	const char* ScriptName(Script a_s)
	{
		switch (a_s) {
		case Script::Japanese: return "Japanese";
		case Script::Korean: return "Korean";
		case Script::Chinese: return "Chinese";
		case Script::Thai: return "Thai";
		case Script::Vietnamese: return "Vietnamese";
		case Script::Cyrillic: return "Cyrillic";
		default: return "none";
		}
	}

	// Dear ImGui's own tables. Chinese uses the 2500 common simplified characters rather than the
	// full 21000: the wheel rasterises at 64 px, and the full set would need an atlas taller than
	// D3D11 allows (4096 x 21000). Everything the translation actually contains is added on top.
	const ImWchar* BuiltInRanges(Script a_s, ImFontAtlas* a_atlas)
	{
		switch (a_s) {
		case Script::Japanese: return a_atlas->GetGlyphRangesJapanese();
		case Script::Korean: return a_atlas->GetGlyphRangesKorean();
		case Script::Chinese: return a_atlas->GetGlyphRangesChineseSimplifiedCommon();
		case Script::Thai: return a_atlas->GetGlyphRangesThai();
		case Script::Vietnamese: return a_atlas->GetGlyphRangesVietnamese();
		case Script::Cyrillic: return a_atlas->GetGlyphRangesCyrillic();
		default: return nullptr;
		}
	}

	bool IsCjk(Script a_s) { return a_s == Script::Japanese || a_s == Script::Korean || a_s == Script::Chinese; }

	// System faces that carry each script, preferred first. Segoe UI already has Cyrillic and
	// Vietnamese; Thai lives in Leelawadee UI and Tahoma.
	std::vector<std::string> FallbackFaces(Script a_s)
	{
		wchar_t winDir[MAX_PATH] = {};
		std::filesystem::path fonts = GetWindowsDirectoryW(winDir, MAX_PATH) > 0 ? std::filesystem::path(winDir) / L"Fonts" : std::filesystem::path(L"C:\\Windows\\Fonts");
		std::vector<const char*> names;
		switch (a_s) {
		case Script::Japanese: names = { "meiryo.ttc", "YuGothM.ttc", "msgothic.ttc", "msyh.ttc" }; break;
		case Script::Korean: names = { "malgun.ttf", "malgunbd.ttf", "msyh.ttc" }; break;
		case Script::Chinese: names = { "msyh.ttc", "simsun.ttc", "meiryo.ttc" }; break;
		case Script::Thai: names = { "leelawui.ttf", "tahoma.ttf" }; break;
		default: break;
		}
		std::vector<std::string> out;
		for (const char* n : names) {
			std::error_code ec;
			const auto p = fonts / n;
			if (std::filesystem::exists(p, ec)) { out.push_back(p.string()); }
		}
		return out;
	}

	std::mutex g_fontStateLock;
	RenderManager::FontState g_fontState;
}

RenderManager::FontState RenderManager::GetFontState()
{
	std::scoped_lock l(g_fontStateLock);
	return g_fontState;
}

void RenderManager::RequestFontRebuild() { fontRebuildPending.store(true); }

void RenderManager::BuildFontAtlas()
{
	INFO("Building font atlas...");
	FontState state;

	// Load font configuration from FontConfig.ini
	std::filesystem::path fontPath;
	bool foundCustomFont = false;
	std::string languageStr;

#define FONTSETTING_PATH "Data\\SKSE\\Plugins\\wheeler\\resources\\fonts\\FontConfig.ini"
	CSimpleIniA ini;
	ini.LoadFile(FONTSETTING_PATH);

	// Read glyph preset from FontConfig.ini (overrides Config::Font::GlyphPreset)
	int glyphPreset = Config::Font::GlyphPreset;
	std::string customRanges = Config::Font::CustomRanges;
	bool logAtlasInfo = Config::Font::Debug::LogAtlasInfo;
	bool showTestOverlay = Config::Font::Debug::ShowGlyphTestOverlay;

	if (const char* presetStr = ini.GetValue("config", "GlyphPreset", nullptr)) {
		glyphPreset = std::atoi(presetStr);
		INFO("[Font] GlyphPreset from INI: {}", glyphPreset);
	}
	if (const char* customRangesStr = ini.GetValue("config", "CustomRanges", nullptr)) {
		customRanges = customRangesStr;
	}
	if (const char* logAtlasStr = ini.GetValue("config.debug", "LogAtlasInfo", nullptr)) {
		logAtlasInfo = (std::string(logAtlasStr) == "true" || std::string(logAtlasStr) == "1");
	}
	if (const char* showOverlayStr = ini.GetValue("config.debug", "ShowGlyphTestOverlay", nullptr)) {
		showTestOverlay = (std::string(showOverlayStr) == "true" || std::string(showOverlayStr) == "1");
		Config::Font::Debug::ShowGlyphTestOverlay = showTestOverlay;  // Update config for runtime
	}

	// The face: the folder named in the INI when it holds a .ttf/.ttc, otherwise a system face.
	if (!ini.IsEmpty()) {
		if (const char* language = ini.GetValue("config", "font", nullptr); language && *language) {
			languageStr = language;
			std::string fontDir = R"(Data\SKSE\Plugins\wheeler\resources\fonts\)" + languageStr;
			std::error_code ec;
			if (std::filesystem::is_directory(fontDir, ec)) {
				for (const auto& entry : std::filesystem::directory_iterator(fontDir, ec)) {
					const auto ext = LowerAscii(entry.path().extension().string());
					if (ext == ".ttf" || ext == ".ttc" || ext == ".otf") {
						fontPath = entry.path();
						foundCustomFont = true;
						break;
					}
				}
			}
			if (foundCustomFont) {
				INFO("[Font] Loading font: {}", fontPath.string());
			} else {
				INFO("[Font] No font found for language: {}", languageStr);
			}
		}
	}
	if (!foundCustomFont) {
		// The owner, 2026-09-13: "a better looking font, something that's not as pixelated". With no
		// custom face configured the wheel used Dear ImGui's 13 px bitmap font scaled up. Every Windows
		// install carries Segoe UI; it is loaded at 64 px like a custom face and scaled down by the
		// drawer, so the labels are hinted TrueType at any size. A FontConfig.ini `font =` still wins.
		wchar_t winDir[MAX_PATH] = {};
		if (GetWindowsDirectoryW(winDir, MAX_PATH) > 0) {
			for (const wchar_t* face : { L"Fonts/segoeui.ttf", L"Fonts/arial.ttf" }) {
				std::filesystem::path candidate = std::filesystem::path(winDir) / face;
				std::error_code ec;
				if (std::filesystem::exists(candidate, ec)) {
					fontPath = candidate;
					foundCustomFont = true;
					INFO("[Font] No custom font configured; using the system face {}", fontPath.string());
					break;
				}
			}
		}
	}

	// The scripts needed: the folder's (case-insensitive) and the game's own language's.
	ImFontAtlas* atlas = ImGui::GetIO().Fonts;
	const Script folderScript = ScriptFor(LowerAscii(languageStr));
	const std::string gameLanguage = Texts::Language();
	const Script gameScript = ScriptFor(gameLanguage);
	INFO("[Font] Folder '{}' -> script {}; game language '{}' -> script {}; GlyphPreset {} ({})",
		languageStr, ScriptName(folderScript), gameLanguage, ScriptName(gameScript), glyphPreset, GlyphRanges::GetPresetName(glyphPreset));

	// The ranges, built rather than picked (see the note at the top of this section).
	static ImVector<ImWchar> s_ranges;
	{
		ImFontGlyphRangesBuilder builder;
		builder.AddRanges(GlyphRanges::GetGlyphRangesForPreset(glyphPreset, customRanges));
		if (const ImWchar* r = BuiltInRanges(folderScript, atlas)) { builder.AddRanges(r); }
		if (gameScript != folderScript) {
			if (const ImWchar* r = BuiltInRanges(gameScript, atlas)) { builder.AddRanges(r); }
		}
		const std::string translated = Texts::AllText();
		builder.AddText(translated.c_str());
		s_ranges.clear();
		builder.BuildRanges(&s_ranges);
	}
	if (logAtlasInfo) {
		GlyphRanges::LogRangeDetails(s_ranges.Data);
	}

	atlas->Clear();
	ImFont* loaded = nullptr;
	const bool cjk = IsCjk(folderScript) || IsCjk(gameScript);
	if (foundCustomFont) {
		ImFontConfig cfg;
		if (cjk) {
			// Thousands of 64 px glyphs: horizontal oversampling would triple the atlas width for
			// no visible gain at this size.
			cfg.OversampleH = 1;
			cfg.OversampleV = 1;
		}
		loaded = atlas->AddFontFromFileTTF(fontPath.string().c_str(), 64.0f, &cfg, s_ranges.Data);
		if (!loaded) { logger::warn("[Font] {} could not be rasterised", fontPath.string()); }
		state.face = fontPath.string();
	}

	// Merge a system face for the scripts the chosen face may lack. MergeMode adds only what is
	// still missing, so a folder font that already carries them is left alone.
	if (loaded) {
		std::vector<Script> needed;
		for (const Script s : { folderScript, gameScript }) {
			if ((IsCjk(s) || s == Script::Thai) && std::find(needed.begin(), needed.end(), s) == needed.end()) { needed.push_back(s); }
		}
		for (const Script s : needed) {
			bool merged = false;
			for (const std::string& face : FallbackFaces(s)) {
				ImFontConfig merge;
				merge.MergeMode = true;
				merge.PixelSnapH = true;
				merge.OversampleH = 1;
				merge.OversampleV = 1;
				if (atlas->AddFontFromFileTTF(face.c_str(), 64.0f, &merge, s_ranges.Data)) {
					INFO("[Font] Merged {} for the {} glyphs the atlas needs", face, ScriptName(s));
					state.merged = face;
					merged = true;
					break;
				}
			}
			if (!merged) { logger::warn("[Font] No system face found for {}; characters the main face lacks will draw as '?'", ScriptName(s)); }
		}
	}

	if (!loaded) {
		logger::warn("[Font] No TrueType face could be loaded; using the built-in bitmap font");
		atlas->AddFontDefault();
	}

	// Build the atlas and log size info
	atlas->Build();

	int atlasWidth = 0, atlasHeight = 0;
	unsigned char* pixels = nullptr;
	atlas->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);
	if (logAtlasInfo) {
		INFO("[Font] Atlas built: {}x{} pixels", atlasWidth, atlasHeight);
		if (atlasWidth > 4096 || atlasHeight > 4096) {
			logger::warn("[Font] Atlas is large ({}x{}). Consider using a smaller glyph preset.", atlasWidth, atlasHeight);
		}
	}

	// What the atlas can actually draw, one probe per script, so a driving op can prove the fix
	// without a screenshot: hiragana A, hangul HAN, the hanzi for water, Cyrillic ZHE, Thai KO KAI.
	if (ImFont* f = loaded ? loaded : atlas->Fonts.empty() ? nullptr : atlas->Fonts[0]) {
		state.hasKana = f->FindGlyphNoFallback(0x3042) != nullptr;
		state.hasHangul = f->FindGlyphNoFallback(0xD55C) != nullptr;
		state.hasHanzi = f->FindGlyphNoFallback(0x6C34) != nullptr;
		state.hasCyrillic = f->FindGlyphNoFallback(0x0416) != nullptr;
		state.hasThai = f->FindGlyphNoFallback(0x0E01) != nullptr;
		state.glyphs = f->Glyphs.Size;
	}
	state.folder = languageStr;
	state.folderScript = ScriptName(folderScript);
	state.gameLanguage = gameLanguage;
	state.gameScript = ScriptName(gameScript);
	state.preset = glyphPreset;
	state.atlasWidth = atlasWidth;
	state.atlasHeight = atlasHeight;
	{
		std::scoped_lock l(g_fontStateLock);
		state.builds = g_fontState.builds + 1;
		g_fontState = state;
	}
	INFO("[Font] Atlas {} built: face {}, merged {}, {} glyphs, kana {} hangul {} hanzi {} cyrillic {} thai {}",
		state.builds, state.face.empty() ? "(built-in)" : state.face, state.merged.empty() ? "(none)" : state.merged, state.glyphs,
		state.hasKana, state.hasHangul, state.hasHanzi, state.hasCyrillic, state.hasThai);
	INFO("...font atlas built");
}
// ========== END FONT ATLAS ==========

void RenderManager::D3DInitHook::thunk()
{
	func();

	INFO("RenderManager: Initializing...");
	// Updated for newer CommonLibSSE-NG: use BSGraphics::Renderer
	auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
	if (!renderer) {
		ERROR("Cannot find BSGraphics::Renderer. Initialization failed!");
		return;
	}

	auto* render_data = RE::BSGraphics::Renderer::GetRendererData();
	if (!render_data) {
		ERROR("Cannot get renderer data. Initialization failed!");
		return;
	}

	INFO("Getting swapchain...");
	auto* window = RE::BSGraphics::Renderer::GetCurrentRenderWindow();
	auto* swapchain = window ? window->swapChain : nullptr;
	if (!swapchain) {
		ERROR("Cannot find swapchain. Initialization failed!");
		return;
	}

	device = reinterpret_cast<ID3D11Device*>(render_data->forwarder);
	Texture::device_ = device;
	context = reinterpret_cast<ID3D11DeviceContext*>(render_data->context);
	
	// Initialize AmmoWheel reskin systems
	AmmoWheelReskin::Renderer::GetSingleton().Init(device);  // Legacy system
	AmmoWheelReskinUnified::ReskinSystem::GetSingleton().Init(device);  // Unified system

	INFO("Initializing ImGui...");
	ImGui::CreateContext();
	auto hwnd = window ? reinterpret_cast<HWND>(window->hWnd) : nullptr;
	if (!hwnd || !ImGui_ImplWin32_Init(hwnd)) {
		ERROR("ImGui initialization failed (Win32)");
		return;
	}
	if (!ImGui_ImplDX11_Init(device, context)) {
		ERROR("ImGui initialization failed (DX11)");
		return;
	}

	INFO("...ImGui Initialized");

	ResolutionScale::Context::GetSingleton().Initialize(reinterpret_cast<IDXGISwapChain*>(swapchain), hwnd);

	initialized.store(true);

	WndProcHook::func = reinterpret_cast<WNDPROC>(
		SetWindowLongPtrA(
			hwnd,
			GWLP_WNDPROC,
			reinterpret_cast<LONG_PTR>(WndProcHook::thunk)));
	if (!WndProcHook::func)
		ERROR("SetWindowLongPtrA failed!");

	BuildFontAtlas();

	INFO("RenderManager: Initialized");

}

void RenderManager::DXGIPresentHook::thunk(std::uint32_t a_p1)
{
	func(a_p1);

	if (!D3DInitHook::initialized.load())
		return;

	// A pending atlas rebuild (translations loaded, language switched) happens here, OUTSIDE a
	// frame: the backend's device objects are dropped first so NewFrame recreates the font texture.
	if (fontRebuildPending.exchange(false)) {
		ImGui_ImplDX11_InvalidateDeviceObjects();
		BuildFontAtlas();
	}

	// prologue
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	// Drain the settings page's input queue here and nowhere else: AFTER the backend NewFrame
	// calls, so our positions land after (and therefore win over) ImGui_ImplWin32_NewFrame's own
	// GetCursorPos poll, and BEFORE ImGui::NewFrame() consumes the event queue.
	//
	// Only while the page is open. Closed, this does not run at all and the wheel's frame is
	// byte-identical to before - which is the point: this context is shared with the wheel, so no
	// global io setting may be changed on its behalf.
	if (SettingsPage::Page::IsOpen()) {
		SettingsPage::PageInput::ProcessQueuedEvents();
	}

	ImGui::NewFrame();

	// do stuff
	RenderManager::draw();

	// epilogue
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

struct ImageSet
{
	std::int32_t my_image_width = 0;
	std::int32_t my_image_height = 0;
	ID3D11ShaderResourceView* my_texture = nullptr;
};


void RenderManager::MessageCallback(SKSE::MessagingInterface::Message* msg)  //CallBack & LoadTextureFromFile should called after resource loaded.
{
	if (msg->type == SKSE::MessagingInterface::kDataLoaded && D3DInitHook::initialized) {
		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = true;
		io.WantSetMousePos = true;
	}
}

bool RenderManager::Install()
{
	static std::atomic_bool installed{ false };
	if (installed.load()) {
		logger::info("RenderManager: Install called again, skipping");
		return true;
	}

	auto g_message = SKSE::GetMessagingInterface();
	if (!g_message) {
		ERROR("Messaging Interface Not Found!");
		return false;
	}

	if (!g_message->RegisterListener(MessageCallback)) {
		ERROR("RenderManager: Failed to register SKSE message callback");
		return false;
	}

	const REL::Relocation<std::uintptr_t> d3dInitHook{ D3DInitHook::id, D3DInitHook::offset };
	const REL::Relocation<std::uintptr_t> presentHook{ DXGIPresentHook::id, DXGIPresentHook::offset };
	if (!ValidateCallHookSite(d3dInitHook.address(), "D3DInit") ||
		!ValidateCallHookSite(presentHook.address(), "DXGIPresent")) {
		return false;
	}

	stl::write_thunk_call<D3DInitHook>();
	stl::write_thunk_call<DXGIPresentHook>();

	installed.store(true);
	logger::info("RenderManager: Installed hooks");

	return true;
}



float RenderManager::GetResolutionScaleWidth()
{
	return ImGui::GetIO().DisplaySize.x / 1920.f;
}

float RenderManager::GetResolutionScaleHeight()
{
	return ImGui::GetIO().DisplaySize.y / 1080.f;
}


void RenderManager::draw()
{
	if (!InitState::IsCoreInitialized() || !InitState::IsDataInitialized()) {
		return;
	}

	// Add UI elements here
	float deltaTime = ImGui::GetIO().DeltaTime;
	ResolutionScale::Context::GetSingleton().Update();
	Wheeler::Update(deltaTime);
	TimeFloatInterpolatorManager::Update(deltaTime);

	// Font glyph test overlay (debug feature)
	if (Config::Font::Debug::ShowGlyphTestOverlay) {
		DrawGlyphTestOverlay();
	}

	// The settings page, drawn LAST so it sits above the wheel - ImGui composites in call order.
	// Costs one boolean test per frame while closed, and it is closed until something opens it:
	// the page is not interactive yet, because this context receives no mouse buttons or keyboard
	// until the input translation step lands.
	AMFLaunch::Tick();  // opens the page once the framework's menu has gone (M3)
	SettingsPage::Page::Draw();
}

void RenderManager::DrawGlyphTestOverlay()
{
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
	                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
	                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
	
	// Position in top-left corner with some padding
	ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
	ImGui::SetNextWindowBgAlpha(0.85f);
	
	if (ImGui::Begin("Font Glyph Test", nullptr, flags)) {
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Wheeler Font Glyph Test");
		ImGui::Separator();
		
		// Current preset info
		ImGui::Text("Preset: %d (%s)", Config::Font::GlyphPreset, 
			GlyphRanges::GetPresetName(Config::Font::GlyphPreset));
		ImGui::Separator();
		
		// Test strings for various European languages
		// Using raw UTF-8 bytes in regular string literals (source file is UTF-8)
		// German: AOU aou ss
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "DE:");
		ImGui::SameLine();
		ImGui::Text("\xc3\x84\xc3\x96\xc3\x9c \xc3\xa4\xc3\xb6\xc3\xbc \xc3\x9f | Ger\xc3\xbc" "st \xc3\x9c" "bung \xc3\x84rger");
		
		// Polish: ACELNOSZ acelnoszzz
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "PL:");
		ImGui::SameLine();
		ImGui::Text("\xc4\x84\xc4\x86\xc4\x98\xc5\x81\xc5\x83\xc3\x93\xc5\x9a\xc5\xb9\xc5\xbb \xc4\x85\xc4\x87\xc4\x99\xc5\x82\xc5\x84\xc3\xb3\xc5\x9b\xc5\xba\xc5\xbc");
		
		// Turkish: GIS iou c
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "TR:");
		ImGui::SameLine();
		ImGui::Text("\xc4\x9e\xc4\xb0\xc5\x9e \xc4\xb1\xc3\xb6\xc3\xbc \xc3\xa7 | T\xc3\xbcrk\xc3\xa7""e");
		
		// Czech: CDNRSTZ cdnrstz
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "CZ:");
		ImGui::SameLine();
		ImGui::Text("\xc4\x8c\xc4\x8e\xc5\x87\xc5\x98\xc5\xa0\xc5\xa4\xc5\xbd \xc4\x8d\xc4\x8f\xc5\x88\xc5\x99\xc5\xa1\xc5\xa5\xc5\xbe | \xc4\x8c""esky");
		
		// Romanian: AAIST aaist
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "RO:");
		ImGui::SameLine();
		ImGui::Text("\xc4\x82\xc3\x82\xc3\x8e\xc8\x98\xc8\x9a \xc4\x83\xc3\xa2\xc3\xae\xc8\x99\xc8\x9b | Rom\xc3\xa2n\xc4\x83");
		
		// Swedish: AAO aao
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "SE:");
		ImGui::SameLine();
		ImGui::Text("\xc3\x85\xc3\x84\xc3\x96 \xc3\xa5\xc3\xa4\xc3\xb6 | Svenska");
		
		// Icelandic: PDAO pdao
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "IS:");
		ImGui::SameLine();
		ImGui::Text("\xc3\x9e\xc3\x90\xc3\x86\xc3\x96 \xc3\xbe\xc3\xb0\xc3\xa6\xc3\xb6 | \xc3\x8dslenska");
		
		// Croatian: CCDSZ ccdsz
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "HR:");
		ImGui::SameLine();
		ImGui::Text("\xc4\x8c\xc4\x86\xc4\x90\xc5\xa0\xc5\xbd \xc4\x8d\xc4\x87\xc4\x91\xc5\xa1\xc5\xbe | Hrvatski");
		
		// Lithuanian: ACEEISUUZ aceeisuuz
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "LT:");
		ImGui::SameLine();
		ImGui::Text("\xc4\x84\xc4\x8c\xc4\x98\xc4\x96\xc4\xae\xc5\xa0\xc5\xb2\xc5\xaa\xc5\xbd \xc4\x85\xc4\x8d\xc4\x99\xc4\x97\xc4\xaf\xc5\xa1\xc5\xb3\xc5\xab\xc5\xbe");
		
		// Latvian: ACEGIKLNORSUZ
		ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "LV:");
		ImGui::SameLine();
		ImGui::Text("\xc4\x80\xc4\x8c\xc4\x92\xc4\xa2\xc4\xaa\xc4\xb6\xc4\xbb\xc5\x85\xc5\x8c\xc5\x96\xc5\xa0\xc5\xaa\xc5\xbd");
		
		ImGui::Separator();
		
		// Common punctuation and currency
		ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.5f, 1.0f), "Symbols:");
		ImGui::Text("Euro: \xe2\x82\xac | Pound: \xc2\xa3 | Yen: \xc2\xa5");
		ImGui::Text("Quotes: \"text\" 'text' <<text>>");
		ImGui::Text("Dash: - \xe2\x80\x93 | Ellipsis: \xe2\x80\xa6");
		
		ImGui::Separator();
		ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[Font.Debug] ShowGlyphTestOverlay = false to hide");
	}
	ImGui::End();
}
