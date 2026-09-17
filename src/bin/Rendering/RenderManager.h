#pragma once

#include <atomic>
#include <string>


/// <summary>
/// RenderManager hooks d3d11 render loop and injects imgui entry point.
/// </summary>
class RenderManager
{
	struct WndProcHook
	{
		static LRESULT thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
		static inline WNDPROC func;
	};

	struct D3DInitHook
	{
		static void thunk();
		static inline REL::Relocation<decltype(thunk)> func;

		static constexpr auto id = REL::RelocationID(75595, 77226);
		static constexpr auto offset = REL::VariantOffset(0x9, 0x275, 0x00);  // VR unknown

		static inline std::atomic<bool> initialized = false;
	};

	struct DXGIPresentHook
	{
		static void thunk(std::uint32_t a_p1);
		static inline REL::Relocation<decltype(thunk)> func;

		static constexpr auto id = REL::RelocationID(75461, 77246);
		static constexpr auto offset = REL::Offset(0x9);
	};


private:
	RenderManager() = delete;

	static void draw();
	static void DrawGlyphTestOverlay();
	static void MessageCallback(SKSE::MessagingInterface::Message* msg);

	static inline bool ShowMeters = false;
	static inline ID3D11Device* device = nullptr;
	static inline ID3D11DeviceContext* context = nullptr;


public:
	static bool Install();

	// 1.2.7: the atlas is built from the folder font, the game's language and the loaded translation
	// (see BuildFontAtlas). RequestFontRebuild asks for a new one before the next frame; GetFontState
	// reports what the last build holds, so a driving op can prove a script draws.
	struct FontState
	{
		std::string face, merged, folder, folderScript, gameLanguage, gameScript;
		int preset = 0, glyphs = 0, atlasWidth = 0, atlasHeight = 0, builds = 0;
		bool hasKana = false, hasHangul = false, hasHanzi = false, hasCyrillic = false, hasThai = false;
	};
	static void BuildFontAtlas();
	static void RequestFontRebuild();
	static FontState GetFontState();
	static inline std::atomic<bool> fontRebuildPending = false;

	static float GetResolutionScaleWidth();   // { return ImGui::GetIO().DisplaySize.x / 1920.f; }
	static float GetResolutionScaleHeight();  //{ return ImGui::GetIO().DisplaySize.y / 1080.f; }

};
