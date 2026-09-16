# Wheeler-Refined - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

## 1.2.4 - 2026-09-16 - untested

### Fixed
- **A modifier value that is not an input code is read as "no modifier" instead of breaking the wheel** (the owner, 2026-09-16: *"maybe we could let wheeler accept either 0 or -1"*). `toggleWheelModifier` means "none" as 0, but -1 is just as reasonable a convention - it is what One Click Power Attack uses - and Unbind Vanilla Controls, which now sets this key from the game's own Controls page, wrote -1 here by mistake. The value is read UNSIGNED, so -1 arrived as 4294967295 rather than as anything obviously wrong: the modifier then matched no button, and the wheel simply stopped opening with nothing logged anywhere to say why. Any value past the end of the SKSE input range (281) is now taken to mean no modifier, and says so in the log once, so a settings writer using the other convention cannot silently disable the wheel.

## 1.2.2 - 2026-09-15 - untested

### Fixed
- arming a keymap row armed EVERY row sharing its INI key, so the wrong row swallowed the press (the owner, 2026-09-15: 'when I unbound it and then press rebind and then D-pad, it remains unbound'). Capture was keyed by the INI key alone, but the key is not unique - toggleWheel, nextItem, prevItem and most others exist in BOTH InputBindings.MKB and InputBindings.GamePad - so arming the gamepad Toggle Wheel row also armed the keyboard one, and whichever drew first consumed the code: the keyboard row took the D-pad press and refused it as a controller button. Rebinding a row to the value it already held looked like it worked because nothing changed either way, which is what made this so hard to see. Capture and the row notices are now keyed by section and key together, on both the framework-hosted page and Wheeler's own overlay.

## 1.2.1 - 2026-09-15 - untested

### Fixed
- a keymap row could not be rebound after it was cleared, because capture accepted a code from ANY device (the owner, 2026-09-15: 'wheeler wont let me rebind the dpad after unbinding it'). With the gamepad Toggle Wheel row empty, the capture took mouse-left (256) and the duplicate-binding check then refused it against Activate Primary, which holds 256 on the keyboard side - so every attempt failed for a reason unrelated to the button being pressed, and the row could never be filled again. The captured code's device must now match the row's device: a controller row takes only controller buttons (code 266 and above) and a keyboard row only keys and mouse buttons, with a refusal notice saying so in all eleven languages. The fault predates the Unbind button; clearing a row is what made it reachable.

## 1.2.0 - 2026-09-15 - untested

### Added
- an Unbind button beside Rebind on every keymap row, in both the framework-hosted page and Wheeler's own overlay (the owner: 'wheeler needs to have an unbind button/key button next to each button bind site'). It writes 0 - the value the readers already treat as not bound and the one the row prints as 'Unbound' - through the same ValueStore write and dispatcher rebuild a rebind uses, and is disabled while the row is already unbound or another row is capturing. Until now a key could only be swapped for another, because a capture that produces 0 is rejected as unbindable, so giving a key back meant editing the INI by hand. The button's label is translated in all eleven languages, and DevBench op wheeler.settings unbind drives the same path.

## 1.1.9 - 2026-09-14 - working

### Fixed
- Max Items Per Slot steps one at a time from 1 (the owner: 'as soon as you move it off of one it goes to 10 ... it's one two three four five and so on'). The slider's minimum was 10 in Wheel Behavior's descriptor and the INI reader clamped the value to 10-64, so the shipped 1 jumped to 10 on the first move and a saved 1 read back as 10; both ranges are now 1-64.

### Removed
- Settings Page Key and Settings Page Button (Gamepad) are gone from Wheeler Controls (the owner: 'we don't need a settings page bound button or a settings page bound key either since it's just through AMF'). They are no longer read from Controls.ini and stay unbound; the settings are reached through the Apocrypha Menu Framework.

### Changed
- Max Items Per Slot ships at 5 (the owner: 'i want the default set to 5 per slot'; was 1). Compiled default, the descriptor's default and wheelBehavior.factory.ini match; a saved value is kept until Reset to defaults.

## 1.1.8 - 2026-09-14 - working

### Fixed
- Every new wheel has slots, and the first wheel is always there (the owner: 'can you make it so that wheeler starts with an 8 slot wheel already made', 'i want every new wheel made to have the same number of slots as the slots slider dictates'). PushWheel and AddWheel - behind the settings page's create-first-wheel button, edit mode's add wheel and the reset - made wheels with no slots; a new wheel now gets the slots slider's count (the active wheel's), eight when there is none. A save holding a wheel with no slots (the owner's test save: a 43-byte record, one wheel with an empty entries list) gets it filled on load. A game started with coc from the main menu sends neither kNewGame nor kPostLoadGame, so the first-wheel check also runs about once a second in a running game (never on the main menu or a loading screen). Falsification episode 41.

## 1.1.7 - 2026-09-14 - working

### Fixed
- The wheel no longer opens while the Apocrypha Menu Framework's menu (or another mod's input-taking framework window) is open (the owner: 'I don't like that you can activate wheeler with d-pad down while AMF is running'). OpenWheeler refuses while the framework reports a blocking window, which covers every way in (keyboard toggle, controller press, D-pad hold), and a D-pad toggle direction is not armed as a hold there, so no delayed tap is replayed into the framework's navigation. Keys captured for rebinding on the framework-hosted page are unaffected.

## 1.1.6 - 2026-09-14 - working

### Fixed
- The Advanced settings toggle now hides the advanced sections' tabs (the owner: 'the advanced settings toggle doesnt hide the advanced settings tabs'). With it off, the sections after Wheeler Controls, Wheel Behavior and Ammo Wheel were still listed in the framework's tabs and only showed a one-line notice, because the framework listed every section it was given at registration. Apocrypha Menu Framework 1.8.3 adds AMF_SetPageVisible; Wheeler calls it at registration and from whichever section is drawing, so turning the toggle off (or loading a preset) removes those tabs on the next frame and turning it on brings them back. With an older framework or SKSE Menu Framework the notice stays as before.

### Removed
- Open In Favorites Menu and Edit Mode In Favorites Menu are gone from Wheeler Controls (the owner: 'wheeloer takes the place of the favorites menu so we dont need the settings to allow wheeler to activate in the favorites menu either'). They are no longer read from Controls.ini and keep upstream's behaviour (both on); Controls.defaults.ini drops them, and Disable Vanilla Favorites Menu's description now says Wheeler takes the menu's place.

## 1.1.5 - 2026-09-14 - working

### Fixed
- Disable Vanilla Favorites Menu did nothing on the controller (the owner: 'the toggle didnt disable the favorites menu'). The check compared the user event looked up with Wheeler's remapped input code (mouse +256, gamepad 266 + GetGamepadIndex) - a code the game's control map does not store, so on the controller (D-pad Up is XInput 0x0001) and the mouse the lookup was always empty and the menu still opened; only the keyboard matched. It now asks ControlMap::GetUserEventName with the button's own code, only in gameplay (IsGameplayContextForPassThrough, so D-pad Up still scrolls lists in menus), never for a wheel toggle or a Wheeler-bound key, and a replayed D-pad tap that would open Favorites is dropped too. The shared userEventName lookup used by movement/pause passthrough is unchanged. Falsification episode 40.

## 1.1.4 - 2026-09-13 - untested

### Changed
- The log level is a setting: `[Log] uLogLevel` in `debug.ini`, 0 = trace (shipped) to 6 = off. Release builds were pinned to info, which left bug reports without the detail the log is for.

## 1.1.3 - 2026-09-13 - untested

### Fixed
- The highlighted item's damage or armour figure was drawn on top of its name ("Iron War9Axe"): the shipped stat text and stat icon offsets were 0/0 while the descriptor's own defaults put them 130 px below the name with the icon beside them at a fifth of the old scale. Compiled defaults and `Styles.defaults.ini` now carry the descriptor's values. An existing `Styles.ini` keeps its old zeros until Reset to defaults is pressed or the two sections are edited.

## 1.1.2 - 2026-09-13 - untested

### Added
- The eleven languages. Every string Wheeler shows - the edit-mode hints, the notifications, the settings-page strings including the presets panel - has a translation file per language at `Data\Interface\Translations\Wheeler_<language>.txt` (English, German, French, Spanish, Italian, Russian, Polish, Czech, Japanese, Chinese, Korean), the same UTF-16 `$Key<TAB>text` layout the menu framework uses, so the framework's font atlas already holds the glyphs. The game's own language setting picks the file; a missing or malformed file leaves English in force and says so in the log. `Texts.ini` and `translations.txt` still work as before (English base and overrides).
- An Advanced settings toggle on Wheeler Controls / General (on by default). Off keeps only the first three sections - Wheeler Controls, Wheel Behavior, Ammo Wheel - and hides the rest: the overlay drops their tabs, the framework-hosted page collapses them to a one-line notice, since the framework lists every section it was given at registration.
- DevBench op `wheeler.page texts` reports the language in force, its file and how many keys it applied, and `lang=<name>` reloads with another language for a proof.

## 1.1.1 - 2026-09-13 - untested

### Added
- Settings presets, at the top of Wheeler Controls / General on both settings surfaces: Reset to defaults (every shipped `*.defaults.ini` / `wheelBehavior.factory.ini` copied back over the live INIs), Reload from INI, and named user presets under `Data\SKSE\Plugins\wheeler\user\presets\<name>\` - save the current settings under a name, load, rename, delete. The seven settings INIs travel in a preset; the resolution-specific layout files do not.
- Presets are global to the install: nothing about them is written into a save, and loading a game changes no settings (the owner dropped the per-save link the same day it was built).
- DevBench op `wheeler.page presets` (sub = list, save, load, rename, delete, reset, reload, select) drives all of it headlessly.

## 1.1.0 - 2026-09-13 - untested

### Changed
- Wheeler's own settings key is retired as a default: the settings live in the Apocrypha Menu Framework, whose own key (F1 by default) opens them. The keyboard and gamepad rows remain as optional shortcuts that open the framework on the Wheeler - Refined page (framework 1.7.7 or newer), unbound unless you set them. The edit-mode hint list shows the framework's key on its Settings row.
- The ammo wheel's gamepad default is D-pad right everywhere (the compiled default and the two reset buttons now agree with the INI). Next Item and Previous Item stay on the D-pad: they act only while the wheel is open, the ammo toggle only while it is closed.
- Disable Vanilla Favorites Menu is on the General tab of Wheeler Controls.
- The ammo wheel does not open while a menu is open; closing it still works from anywhere.

## 1.0.9 - 2026-09-13 - untested

### Fixed
- Inside the inventory a tap on the D-pad toggle now scrolls the list: the replayed press carries the user event of the menu's own input context (the game's context stack), not the gameplay one.
- The gamepad edit-mode defaults for Move Slot Forward / Back sat on D-pad up / down, the same button as the wheel toggle; they now ship unbound (rebindable), one key one action.

### Changed
- The D-pad hold-to-open test now applies only inside menus. In gameplay a D-pad toggle press opens the wheel immediately and is the wheel's press, never passed to the game or another mod's hotkey.
- Wheel labels use a clear system font (Segoe UI, then Arial) when no custom font is configured, instead of the 13-pixel ImGui bitmap font scaled up. A FontConfig.ini font still takes precedence.
- Max Items Per Slot defaults to 1.
- The edit-mode hint panel is anchored at 0/0 (the top-left corner) by default and lists more of the controls: Open / Close Wheel, Next Item, Previous Item, Show / Hide These Hints.
- The Ammo Wheel panel is the third section, right after Wheel Behavior.

### Added
- A Number Of Wheels slider next to Slots On This Wheel, on both the framework page and the overlay: growing adds empty wheels with the same slot count; shrinking removes from the end and stops at a wheel that still holds an item. Driving op wheeler.page wheels.
- The centre label's exact text is logged when it changes, to pin down the stray digit seen in a screenshot ("Iron War9Axe").

## 1.0.8 - 2026-09-13 - untested

### Changed
- The settings are a Wheeler mod menu inside the Apocrypha Menu Framework: every settings panel is a section under "Wheeler - Refined" in the framework's menu, drawn with the framework's own ImGui and theme, with the same tabs, switches, sliders, colours, dropdowns, key rows (reserved-key and one-key-one-action refusals included) and buttons. The "open the Wheeler page" button the framework used to show is gone. Wheeler's own overlay page remains only as the fallback when no framework is loaded; with one loaded, the settings key shows a message saying where the settings are.

## 1.0.7 - 2026-09-13 - untested

### Added
- A first wheel with eight empty slots is created on a new game and on any loaded save that has no wheel, so the wheel exists the moment the mod is installed instead of after a trip into edit mode.
- A gamepad row for the settings page: `Control.Wheel/SettingsPageGamepadButton`, rebindable on the Wheeler Controls tab, shown on the edit-mode hint list next to the keyboard key. Ships unbound; the page is also reachable from the menu framework's Mod Control Panel with a controller.

### Fixed
- The wheel drew nothing at all. The shipped Styles.defaults.ini set UseGeometricPrimitiveForBackgroundTexture to false, which makes the wheel draw its slots from slot_background.svg and wheel_background.svg, files that no release of Wheeler or Wheeler Refined ships; with no texture pack installed the wheel was invisible while every key still worked. The shipped default is now true, the value the original mod ships and the settings page's own text asks for. A Styles.ini written by an earlier test build keeps false; switch Render Texture From Geometric Primitives on, or delete that file.

### Changed
- The wheel is anchored to the screen centre; Wheel Center Offset X and Y move it from there in 1080p pixels scaled once for the display, so the same numbers work at every resolution. The inherited default of 450 (and a second scaling pass on top of it) had put the wheel half off the right edge at 3200x1800. Both offsets now default to 0.
- The settings page opens on Wheeler Controls, then Wheel Behavior, Styles, Ammo Wheel, I4, Action Hotkeys Bridge, OStim Integration, with the legacy layout-reset helper last. It opened on the legacy helper because the panels were in file-name order.

## 1.0.6 - 2026-09-13 - untested

### Added
- D-pad toggle: hold to open, tap for the game. When the gamepad toggle is a D-pad direction with no modifier, holding it past Toggle Hold Threshold opens the wheel and a shorter tap is replayed to the game or to the open menu as an ordinary D-pad press, so the D-pad keeps its normal function in gameplay AND in the inventory. Setting `Control.Wheel/DpadHoldToToggle` (on by default) on the Wheeler Controls page; other buttons keep press-to-toggle. With the wheel open the toggle press closes it and is consumed.

### Changed
- The Toggle Key Keeps Its Game Function description says that a D-pad direction is decided by duration instead of being shared.
- The shipped INI's comment for SettingsPageKey now describes F10.

## 1.0.5 - 2026-09-12 - untested

### Changed
- Default keys: the settings page opens on F10 instead of F11 (F11 is Apocrypha Menu Framework's menu key), and the gamepad toggle defaults to D-pad down with no modifier instead of X held with the left bumper. Existing user INIs keep their own values.
- The compiled keyboard toggle default is G (34) to match the shipped INI; it was caps lock (58), a value the INI never used.

### Added
- One key, one action: a rebind that lands on a key another Wheeler action already holds on the same device is refused, and the row names the action that has it.
- Reserved-key respect: Wheeler asks the menu framework which keys it reserves (SMF_GetReservedKeyCodes, resolved by GetProcAddress with a null check) and never takes them. A rebind that lands on one is refused with a message on the row, and a settings-page key or keyboard wheel toggle that sits on one at load is left unbound with a warning in wheeler.log.

## 1.0.4 - 2026-09-12 - untested

### Changed
- The toggle key keeps its game function in gameplay only. Inside a menu - inventory, magic, container, barter, favorites, crafting, journal, map, or any menu that pauses the game - the press belongs to Wheeler outright, so a D-pad direction bound to open the wheel in the inventory opens it without also moving the list.

## 1.0.3 - 2026-09-12 - untested

### Added
- When no wheel exists yet, the Wheel Behavior panel offers 'Create the first wheel', so a fresh game gets its first wheel from the settings page instead of the Favorites-menu edit mode; a wheel with no slots is then grown with the slider.
- A 'Slots on this wheel' slider (1 to 64) at the top of the Wheel Behavior panel sets the loaded wheel's slot count in one move instead of adding or removing slots one at a time on the controller. Growing adds empty slots; shrinking removes empty slots from the end and stops at the first slot that still holds an item, so nothing placed is discarded. The driving tool's 'slots' operation reads and sets it.

### Changed

## 1.0.2 - 2026-09-12 - untested

### Added
- The wheeler.page driving tool gained spy (switch the input spy and menu-block logging on at runtime; a config reload resets them to the INI), and inject (a REAL button event spliced ahead of Wheeler's own input filter - an InputBench press never reaches it, because both hook the same call site and Wheeler, loading later, wraps it and filters the raw list first).
- New setting Control.Wheel/ToggleKeyPassThrough (default on): the wheel's toggle button keeps its normal game function - the unchorded press that opens or closes the wheel, and its release, are handed to the game instead of being consumed by the main-wheel input lock. A chorded toggle is unaffected. On the settings page as 'Toggle Key Keeps Its Game Function'.

### Fixed
- The Toggle Key Keeps Its Game Function setting is proven: with it on, an unchorded toggle press leaves the filter unconsumed (the game keeps the event); with it off, the same press is consumed by the main-wheel input lock. Note the fork's defaults chord the gamepad toggle with LB (toggleWheelModifier = 274); a chorded toggle is consumed either way, so clear the modifier for a D-pad direction that should keep its game function.

## 1.0.1 - 2026-09-12 - untested

### Added
- The settings page is reachable from Apocrypha Menu Framework's Mod Control Panel: an entry 'Wheeler - Refined / Settings' registers through AMF_RegisterPage when the framework is loaded, and its button arms a launch that opens Wheeler's own page once the framework menu closes. No framework header is vendored; the boundary is GetProcAddress, and registration is refused unless every export resolves.

### Changed
- dMenu is no longer referenced anywhere: the menu-open checks that kept the wheel up during live editing, the ammo wheel's blocking list, the input spy's menu list and the edit-mode 'Settings' hint now use Wheeler's own settings page and its SettingsPageKey binding. The dmenu_updateSettings mod-callback listener is kept so an INI written by any external tool still reloads live.