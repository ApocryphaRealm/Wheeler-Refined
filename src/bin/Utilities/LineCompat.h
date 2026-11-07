#pragma once

// Build-line compatibility (2026-11-07). Wheeler builds twice from one source: RUNTIME_LINE 1 on the pinned
// CommonLibSSE-NG b93280e (SE 1.5.97 / AE 1.6.x) and RUNTIME_LINE 17 on CommonLibSSE-NG 7.2.0 (Skyrim 1.7.x). Where
// the two libraries spell the same engine data differently, the difference lives here, once.
//
// Both are ACCESSORS on line 17 for a reason, not only a rename: on 1.7.99+ SkyrimVM's impl moved from +0x200 to
// +0x210, so a direct member read compiled against one layout would read the wrong slot on the other game.

namespace LineCompat
{
	// The Papyrus virtual machine behind SkyrimVM, or nullptr.
	inline RE::BSScript::IVirtualMachine* VM(RE::SkyrimVM* a_skyrimVM)
	{
		if (!a_skyrimVM) {
			return nullptr;
		}
#if RUNTIME_LINE == 17
		return a_skyrimVM->GetVMRuntimeData().impl.get();
#else
		return a_skyrimVM->impl.get();
#endif
	}

	// A HUD notification (top-left message). 7.2 has no RE::DebugNotification; SendHUDMessage::ShowHUDMessage is the
	// same game function.
	inline void Notify(const char* a_text)
	{
#if RUNTIME_LINE == 17
		RE::SendHUDMessage::ShowHUDMessage(a_text, nullptr, true);
#else
		RE::DebugNotification(a_text);
#endif
	}

	// A sound handle from a sound descriptor's editor ID (7.2 renamed BuildSoundDataFromEditorID).
	inline void SoundFromEditorID(RE::BSAudioManager* a_audio, RE::BSSoundHandle& a_handle, const char* a_editorID, std::uint32_t a_flags)
	{
#if RUNTIME_LINE == 17
		a_audio->GetSoundHandleByName(a_handle, a_editorID, a_flags);
#else
		a_audio->BuildSoundDataFromEditorID(a_handle, a_editorID, a_flags);
#endif
	}

	// Moves an actor value's DAMAGE modifier by a_delta: negative damages (spends magicka), positive restores. Line 1
	// spells it RestoreActorValue(kDamage, av, delta); 7.2 has two-argument Restore/Damage that each take a positive
	// amount (past-mistakes entry 12866, Njordlinger 1.0).
	inline void ModDamage(RE::ActorValueOwner* a_owner, RE::ActorValue a_av, float a_delta)
	{
#if RUNTIME_LINE == 17
		if (a_delta >= 0.0f) {
			a_owner->RestoreActorValue(a_av, a_delta);
		} else {
			a_owner->DamageActorValue(a_av, -a_delta);
		}
#else
		a_owner->RestoreActorValue(RE::ACTOR_VALUE_MODIFIER::kDamage, a_av, a_delta);
#endif
	}

	// Enables or disables a control group without storing the previous state (7.2 adds the a_storeState argument).
	inline void ToggleControls(RE::ControlMap* a_map, RE::ControlMap::UEFlag a_flags, bool a_enable)
	{
#if RUNTIME_LINE == 17
		a_map->ToggleControls(a_flags, a_enable, false);
#else
		a_map->ToggleControls(a_flags, a_enable);
#endif
	}

	// An empty item card for the game's "describe this magic item" function. 7.2's RE::ItemCard has only a constructor
	// that takes a movie view and creates a Scaleform object in it; the game function needs neither, only the 0x40 bytes
	// line 1 default-constructs. Same members, same layout, checked below.
	class BlankItemCard
	{
	public:
		RE::ItemCard* get() noexcept { return reinterpret_cast<RE::ItemCard*>(&_card); }

	private:
#if RUNTIME_LINE == 17
		struct Layout
		{
			RE::GFxValue                   obj;
			RE::BSString                   infoText;
			void*                          unk28{ nullptr };
			std::uint32_t                  unk30{ 0 };
			std::uint32_t                  pad34{ 0 };
			RE::GPtr<RE::GFxMovieView>     view;
		};
		static_assert(sizeof(Layout) == sizeof(RE::ItemCard));
		static_assert(offsetof(Layout, infoText) == offsetof(RE::ItemCard, infoText));
		static_assert(offsetof(Layout, view) == offsetof(RE::ItemCard, view));
		Layout _card;
#else
		RE::ItemCard _card;
#endif
	};

	// The renderer's data block (device, context, swap chain), or nullptr.
	inline auto* RendererData()
	{
#if RUNTIME_LINE == 17
		return RE::BSGraphics::Renderer::GetRendererDataSingleton();
#else
		return RE::BSGraphics::Renderer::GetRendererData();
#endif
	}
}
