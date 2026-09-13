# Wheeler-Refined - changelog

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

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