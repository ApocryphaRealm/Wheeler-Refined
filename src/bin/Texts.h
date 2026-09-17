#pragma once
class Texts
{
public:
	enum class TextType
	{
		AlchemyDynamicIDConsumptionWarning,
		NoWheelPresent,
		EditHintTitle,
		EditHintDeviceHeaderMkb,
		EditHintDeviceHeaderGamepad,
		EditHintActionUsePlaceItem,
		EditHintActionRemoveItemWheel,
		EditHintActionAddEmptySlot,
		EditHintActionAddWheel,
		EditHintActionNextWheel,
		EditHintActionPreviousWheel,
		EditHintActionMoveSlotForward,
		EditHintActionMoveSlotBack,
		EditHintActionMoveWheelForward,
		EditHintActionMoveWheelBack,
		EditHintActionSettingsDMenu,
		AmfLaunchExplain,
		AmfLaunchButton,
		AmfLaunchPending,
		SlotCountLabel,
		SlotCountHelp,
		SlotCountNoWheel,
		SlotCountCreateFirst,
		EditHintActionExitWheel,
		EditHintNavToggleMkb,
		EditHintNavToggleGamepad,
		
		// Notifications
		WheelBehaviorDefaultsSaved,
		WheelBehaviorRestoredToDefaults,
		NoWheelBehaviorDefaultsSaved,
		PressKeyToBindAmmoWheel,
		PressGamepadButtonToBindAmmoWheel,
		KeybindCaptureCancelled,
		AmmoWheelKeybindsReset,
		PressKeyToSetModifier,
		KeyboardModifierCleared,
		PressGamepadButtonToSetModifier,
		GamepadModifierCleared,
		ClickMouseButtonToBind,
		MouseToggleCleared,
		AmmoWheelFactoryDefaultsRestored,
		AmmoWheelFactoryDefaultsFailed,
		InsufficientMagickaForInstantCast,

		// M7 - slots-per-wheel slider. Added BEFORE Total: it is a sentinel and must stay last.
		SlotCountBlockedByFilledSlot,
		SlotCountAtMaximum,
		KeymapReservedByFramework,   // a rebind landed on a key the menu framework reserves
		KeymapAlreadyBoundTo,        // a rebind landed on a key another Wheeler action holds; the action's name follows
		KeymapUnbindButton,          // the Unbind button beside Rebind on every keymap row
		KeymapWrongDevice,           // a rebind captured a code from the wrong device for that row
		SettingsLiveInFramework,     // the settings key was pressed while the framework hosts the page
		EditHintActionToggleWheel,
		EditHintActionNextItem,
		EditHintActionPreviousItem,
		EditHintActionToggleHints,
		WheelCountLabel,
		WheelCountHelp,
		WheelCountBlockedByFilledWheel,
		PresetsHeader,
		PresetsHelp,
		PresetLabel,
		PresetNone,
		PresetNameLabel,
		PresetSaveAs,
		PresetRename,
		PresetDelete,
		PresetResetDefaults,
		PresetReloadIni,
		PresetSaved,
		PresetLoaded,
		PresetRenamed,
		PresetDeleted,
		PresetNoneSelected,
		PresetDefaultsRestored,
		PresetReloaded,
		PresetInvalidName,
		PresetNotFound,
		PresetAlreadyExists,
		AdvancedSettingsHidden,

		Total
	};

	static void LoadTranslations();
	// 1.1.2 (rule 66, the eleven languages): after Texts.ini and translations.txt (English), the file
	// Data\Interface\Translations\Wheeler_<language>.txt for the game's language (sLanguage:General)
	// overrides every key it carries. Same layout and format as the menu framework's own files
	// (UTF-16LE, "$Key<TAB>text"), so the framework's font atlas already holds the glyphs.
	// a_forceLanguage (a driving op) reloads with that language instead of the game's.
	static void LoadLanguageFile(const std::string& a_forceLanguage = std::string());
	static const std::string& Language();
	// Every string currently in force, concatenated: the font atlas is built from it (1.2.7).
	static std::string AllText();
	static const std::string& LanguageFile();
	static int LanguageEntries();
	static const char* GetText(TextType a_textType);

private:
#define MAP_ENTRY(textTypeName, defaultText) \
	{                              \
		TextType::textTypeName, defaultText\
	}

	static inline std::string _language = "english";
	static inline std::string _languageFile;
	static inline int _languageEntries = 0;
	// The English texts as loaded from Texts.ini + translations.txt, so a language reload starts clean.
	static inline std::unordered_map<TextType, std::string> _englishData;
	static inline std::unordered_map<TextType, std::string> _textData = {
		MAP_ENTRY(AlchemyDynamicIDConsumptionWarning, ""),
		MAP_ENTRY(NoWheelPresent, ""),
		MAP_ENTRY(EditHintTitle, "Edit Mode Controls"),
		MAP_ENTRY(EditHintDeviceHeaderMkb, "KB/M"),
		MAP_ENTRY(EditHintDeviceHeaderGamepad, "Gamepad"),
		MAP_ENTRY(EditHintActionUsePlaceItem, "Use / Place Item"),
		MAP_ENTRY(EditHintActionRemoveItemWheel, "Remove Item / Wheel"),
		MAP_ENTRY(EditHintActionAddEmptySlot, "Add Empty Slot"),
		MAP_ENTRY(EditHintActionAddWheel, "Add Wheel"),
		MAP_ENTRY(EditHintActionNextWheel, "Next Wheel"),
		MAP_ENTRY(EditHintActionPreviousWheel, "Previous Wheel"),
		MAP_ENTRY(EditHintActionMoveSlotForward, "Move Slot Forward"),
		MAP_ENTRY(EditHintActionMoveSlotBack, "Move Slot Back"),
		MAP_ENTRY(EditHintActionMoveWheelForward, "Move Wheel Forward"),
		MAP_ENTRY(EditHintActionMoveWheelBack, "Move Wheel Back"),
		MAP_ENTRY(EditHintActionSettingsDMenu, "Settings"),
		MAP_ENTRY(AmfLaunchExplain, "Wheeler draws its settings page in its own overlay, not inside this menu. Press the button, then close this menu: the Wheeler page opens as soon as it is gone."),
		MAP_ENTRY(AmfLaunchButton, "Open the Wheeler settings page"),
		MAP_ENTRY(AmfLaunchPending, "Ready - close this menu and the Wheeler page will open."),
		MAP_ENTRY(SlotCountLabel, "Slots on this wheel"),
		MAP_ENTRY(SlotCountHelp, "Set how many slots the current wheel has (1 to 64). Growing adds empty slots; shrinking removes empty slots from the end and stops at the first slot that still holds an item, so nothing you placed is ever discarded."),
		MAP_ENTRY(SlotCountNoWheel, "No wheel exists yet."),
		MAP_ENTRY(SlotCountCreateFirst, "Create the first wheel"),
		MAP_ENTRY(EditHintActionExitWheel, "Exit Wheel"),
		MAP_ENTRY(EditHintNavToggleMkb, "H for keybind hints"),
		MAP_ENTRY(EditHintNavToggleGamepad, "Left Stick Button for keybind hints"),
		MAP_ENTRY(WheelBehaviorDefaultsSaved, "Wheeler: Wheel Behavior defaults saved."),
		MAP_ENTRY(WheelBehaviorRestoredToDefaults, "Wheeler: Wheel Behavior restored to defaults."),
		MAP_ENTRY(NoWheelBehaviorDefaultsSaved, "Wheeler: Failed to restore Wheel Behavior factory defaults."),
		MAP_ENTRY(PressKeyToBindAmmoWheel, "Wheeler: press a key to bind Ammo Wheel."),
		MAP_ENTRY(PressGamepadButtonToBindAmmoWheel, "Wheeler: press a gamepad button to bind Ammo Wheel."),
		MAP_ENTRY(KeybindCaptureCancelled, "Wheeler: keybind capture cancelled."),
		MAP_ENTRY(AmmoWheelKeybindsReset, "Wheeler: Ammo Wheel keybinds reset."),
		MAP_ENTRY(PressKeyToSetModifier, "Wheeler: press a key to set as modifier (ESC to cancel)."),
		MAP_ENTRY(KeyboardModifierCleared, "Wheeler: Keyboard modifier cleared."),
		MAP_ENTRY(PressGamepadButtonToSetModifier, "Wheeler: press a gamepad button to set as modifier."),
		MAP_ENTRY(GamepadModifierCleared, "Wheeler: Gamepad modifier cleared."),
		MAP_ENTRY(ClickMouseButtonToBind, "Wheeler: click a mouse button to bind."),
		MAP_ENTRY(MouseToggleCleared, "Wheeler: Mouse toggle cleared."),
		MAP_ENTRY(SlotCountBlockedByFilledSlot, "Wheeler: stopped - the next slot still holds an item. Empty it first."),
		MAP_ENTRY(SlotCountAtMaximum, "Wheeler: 64 slots is the most a wheel can be saved with."),
		MAP_ENTRY(KeymapReservedByFramework, "not bound - the menu framework reserves that key"),
		MAP_ENTRY(KeymapAlreadyBoundTo, "not bound - that key is already used by"),
		MAP_ENTRY(KeymapUnbindButton, "Unbind"),
		MAP_ENTRY(KeymapWrongDevice, "not bound - press a control on this row's own device"),
		MAP_ENTRY(SettingsLiveInFramework, "Wheeler: the settings are in the Apocrypha Menu Framework menu - open it and choose Wheeler - Refined."),
		MAP_ENTRY(EditHintActionToggleWheel, "Open / Close Wheel"),
		MAP_ENTRY(EditHintActionNextItem, "Next Item"),
		MAP_ENTRY(EditHintActionPreviousItem, "Previous Item"),
		MAP_ENTRY(EditHintActionToggleHints, "Show / Hide These Hints"),
		MAP_ENTRY(WheelCountLabel, "Number of wheels"),
		MAP_ENTRY(WheelCountHelp, "How many wheels you have (1 to 100). Growing adds empty wheels with the same number of slots; shrinking removes wheels from the end and stops at the first wheel that still holds an item."),
		MAP_ENTRY(WheelCountBlockedByFilledWheel, "Wheeler: stopped - the last wheel still holds an item. Empty it first."),
		MAP_ENTRY(PresetsHeader, "Settings presets"),
		MAP_ENTRY(PresetsHelp, "Save your settings under a name and load them back whenever you like. Presets are files of your own; the shipped defaults are always one button away."),
		MAP_ENTRY(PresetLabel, "Preset"),
		MAP_ENTRY(PresetNone, "(none)"),
		MAP_ENTRY(PresetNameLabel, "Preset name"),
		MAP_ENTRY(PresetSaveAs, "Save current settings as"),
		MAP_ENTRY(PresetRename, "Rename to that name"),
		MAP_ENTRY(PresetDelete, "Delete"),
		MAP_ENTRY(PresetResetDefaults, "Reset to defaults"),
		MAP_ENTRY(PresetReloadIni, "Reload from INI"),
		MAP_ENTRY(PresetSaved, "Saved preset:"),
		MAP_ENTRY(PresetLoaded, "Loaded preset:"),
		MAP_ENTRY(PresetRenamed, "Renamed preset to:"),
		MAP_ENTRY(PresetDeleted, "Deleted preset:"),
		MAP_ENTRY(PresetNoneSelected, "No preset selected."),
		MAP_ENTRY(PresetDefaultsRestored, "Every settings file is back to its shipped defaults."),
		MAP_ENTRY(PresetReloaded, "Settings reloaded from the INI files."),
		MAP_ENTRY(PresetInvalidName, "Use a name of 1 to 40 letters, digits, spaces, - _ ( ) or ."),
		MAP_ENTRY(PresetNotFound, "No such preset:"),
		MAP_ENTRY(PresetAlreadyExists, "A preset already has that name:"),
		MAP_ENTRY(AdvancedSettingsHidden, "Advanced settings are off. Turn them on under Wheeler Controls / General to show this section."),
		MAP_ENTRY(AmmoWheelFactoryDefaultsRestored, "Wheeler: Ammo Wheel restored to factory defaults."),
		MAP_ENTRY(AmmoWheelFactoryDefaultsFailed, "Wheeler: Failed to restore Ammo Wheel factory defaults."),
		MAP_ENTRY(InsufficientMagickaForInstantCast, "Not enough magicka for instant cast.")
	};
};
