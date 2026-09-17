# Perfected Wheeler - Apocrypha Menu Framework (repo Wheeler-Refined) - changelog

Published as "Wheeler - Refined" (ApocryphaRealm fork) up to 1.2.9; renamed with 1.3.0 (the owner, 2026-09-17: *"this is the new name i want it to go under with wheeler refined as the upstream and inspiration"*).

Written as changes happen, not reconstructed afterwards (rule 61). Each version carries its
**version-ledger status**, so this file cannot quietly claim more than the ledger does:

* **working** - observed running in game
* **untested** - built and packaged, not yet confirmed
* **failed** - built but crashed or malfunctioned; the number was reclaimed
* **scratch** - a hypothesis-test build that never held a real number

## 1.3.0 - 2026-09-17 - working

### Added
- **Perfected Wheeler Favorites System** - one master toggle on the General tab (Wheeler Controls), on by default, that gates everything below: off, Wheeler behaves exactly as 1.2.9 did (the owner: *"make sure that all of the changes we're making is gated on the favorites system toggle ... so it doesn't bleed into the non favorites wheeler"*). It lives in Controls.ini, so the presets carry it.
- **Favorites go to designated slots** (the owner on the Nexus page, 2026-09-17: *"a toggle that adds a new system that allows you to set what each slot holds and when you favorite an item it goes to the designated slot immediately which makes it more in line with being a replacement for vanilla favorites menu"*, and *"make this its own sub tab under wheeler behavior tab"*). **Two wheels ship on install** now (the owner: *"a second separate wheel that's built in as already made like the current one on install and give it the same treatment but for the magic menu and its skyui categories"*): the **Inventory Wheel** with ten slots and the **Magic Wheel** with five, named under the wheel indicator and remembered by role in the save. Opening the wheel inside the Magic menu shows the Magic Wheel, inside the Inventory menu the Inventory Wheel, and closing it puts back the wheel you had, so gameplay does not inherit the menu's wheel (the owner: *"these labels aren't cosmetic only, we should also make it so that the magic wheel is opened by default in the magic menu"*). A new **Favorites Menu** tab, first under Wheel Behavior: a category for each slot position of each wheel. The categories are the SkyUI tabs - Weapons, Apparel, Potions (poisons included), Scrolls, Food, Ingredients, Spells, Powers, Shouts - plus Worn Armor, Ammo, Torch and Any; slot 1 is the bottom of the wheel and the numbers run clockwise. The Inventory Wheel's defaults from the bottom are Powers (locked), Melee Weapons, Ranged Weapons (bows, crossbows, staffs), Apparel, Potions, Shouts (the top slot, locked), Scrolls, Food, Ingredients and Misc (torches and other equippable misc items) (the owner: *"as a preliminary default, we set it so that each tab corresponds to one category"*, *"powers go to the bottom of the wheel visually and shouts go to the top visually"*, *"ranged weapons should get their own slot separate from melee and ammo wheel makes ammo getting a slot irrelevant"*, *"misc category gets a slot for equippable items like the torch"*); the two locked slots take their category whatever their settings say and Pick Up / Drop leaves them in place. The Magic Wheel's are the five schools in SkyUI order: Alteration, Illusion, Destruction, Conjuration, Restoration. A spell is filed under the school of its costliest effect, as the magic menu files it. A new wheel made later gets ten slots while the system is on. Apparel excludes the four main worn pieces (head, body, hands, feet), which are the separate Worn Armor category and assigned to no slot by default - shields, rings, amulets, cloaks and a mod's lantern are apparel and land in the apparel slot (the owner: *"there are armor category items that aren't worn on those slots like the shield and lanterns from other mods"*). With the toggle on, favouriting something in the inventory, magic, container or barter menu puts it straight into the first slot on any wheel whose category takes it - the Inventory Wheel then the Magic Wheel for an exact match first, then a slot marked Any - with a HUD line saying which wheel and slot. The system ships on. A Spells slot takes any school; a Shouts and Powers slot takes both. Worn armour is the four main pieces - a helmet counts by its hair slot too, as vanilla helmets are built. Favourites you already had stay where they are: the first read after a game loads only takes the snapshot, so only what is favourited from then on moves. Un-favouriting removes nothing. A form already anywhere on the receiving wheel is not added twice, and a slot at Max Items Per Slot says it is full. The read runs on the game thread every 20 frames while one of those menus is open and once more when it closes. The settings live in wheelBehavior.ini, so the presets on the General tab save and restore them with everything else.
- The driving tool gained `op=favorites` (`sub=scan` forces a read; `sub=mark` and `sub=adopt` are test-only ways to favourite items and to route every current favourite; the state carries the categories, the resolved wheel indexes, counts and the last action), `op=wheels sub=reset|activate`, and the `op=slot` readout names the wheel and no longer says Unknown for spells and shouts.
- **Renamed to Perfected Wheeler - Apocrypha Menu Framework** for the page, the package and the distro folder; the repo, the ledger key and every file name inside the package stay as they were. The binary's own display string still reads Wheeler - Refined in this version (the proven DLL was not rebuilt after the proof).

### Proven
- 2026-09-17 18:29-18:33 on Njordlinger Test (SE 1.5.97), driven through DevBench on the owner's populated save: the two named wheels made with 10 and 5 slots, 31 favourites routed to the expected slots and 19 left alone for the expected reasons (worn armour, ammo, gold), the Magic menu opening on the Magic Wheel and the Inventory Wheel restored on close, the locked slots refusing pick-up and drop, the master toggle off routing nothing. The owner's own rounds earlier that day drove the corrections (helmet biped mask, slot 1 is the bottom, powers and shouts split, ranged weapons, misc).

## 1.2.9 - 2026-09-17 - untested

### Added
- **The Address Library check speaks before CommonLib can fail** (oproso on the Nexus page, 16-17 Sep 2026, Fluorine on SteamOS: *"wheeler.dll - CommonLibSSE-NG/include/REL/ID(219): failed to open the address library file"*, the same from our CNO patch). That line is CommonLibSSE-NG's, raised the first time an address is resolved, and it names neither the file it wanted, the folder it looked in nor the game version it decided on - under Proton any of the three can be the wrong one. Before this plugin resolves anything it now makes the same decision CommonLib will make and writes it to the log: the runtime detected and the executable it was read from, the exact file (`versionlib-<v>.bin` for AE, `version-<v>.bin` for SE), the working directory that path is relative to, and whether the file is there - and whether it is there beside the executable instead, which would mean the working directory is the problem. When it is missing, a message names all of that and the plugin loads inert; the game carries on. `src/bin/AddressLibraryGuard.h` is self-contained so every mod of ours on CommonLibSSE-NG can carry it.

## 1.2.8 - 2026-09-17 - untested

### Added
- **Pick Up / Drop Slot** (Barbadoza on the Nexus page, 2026-09-17, asking to reorient a loaded wheel without rebuilding it; the owner's shape of it: *"click the slot with r3 and move it to the desired position then click to release"*). In edit mode, one press picks the highlighted slot up with everything in it - the slot keeps its highlight while the cursor moves away, so the hand is visible - and a second press on another slot drops it there: the two slots swap places, full or empty, so nothing else on the wheel moves (the owner, 2026-09-17: *"make it swap places if another full slot is selected to move the current slot to"*). A press on the slot in hand puts it back, and so does closing the wheel or changing wheel. Defaults: **V** on the keyboard, **R3** (right stick click) on the controller; both rebindable on the Wheeler Controls page, and the edit-mode hints list the new row. A slot removed or added while one is in hand puts the hand down first, because the slot numbers change under it.
- The two controller buttons for moving a whole WHEEL forward and back now ship unbound (still rebindable): R3 belongs to the new bind, and the back button had been sharing L3 with the hints toggle since upstream, which the load-time collision report of 1.2.5 would have named on every start.
- The driving tool gained `op=slot` (`sub=hover index=N`, `sub=pickup`, or no sub to read): the active wheel's slots in order, plus the hovered and held indices, so a pick-up and drop is proved by reading the order before and after. A Controls.ini from before 1.2.8 that still has Move Wheel Forward on R3 (the old default) is updated on load, with a line in the log, so the new bind is not dead on an existing install.

## 1.2.7 - 2026-09-17 - untested

### Fixed
- **Japanese (and every non-Latin script) draws on the wheel** (littlefot on the Nexus page, 2026-09-17: with `font = japanese` the Japanese face loaded, the log read *"Language 'japanese' - using GlyphPreset 3 (Latin Full)"*, and 火炎 / 治癒 came out as `????`). The folder name in FontConfig.ini was compared case-sensitively against `Japanese`, so a lower-case folder - which Windows opens just the same - fell through to the Latin presets and the atlas held no kana or kanji at all. The atlas is now BUILT rather than picked: the preset the player chose (the English fallback strings need it), the built-in ranges of the folder's script matched without regard to case, the built-in ranges of the GAME's language (a Japanese game shows Japanese item names on the wheel whatever the folder says), and every character of the loaded translation. When the chosen face lacks the script - Segoe UI has no kana, hangul or hanzi - a Windows face that has it is merged in for the missing glyphs (Meiryo / Yu Gothic / MS Gothic for Japanese, Malgun Gothic for Korean, Microsoft YaHei / SimSun for Chinese, Leelawadee UI for Thai), so no font folder is needed for those languages any more. The atlas is rebuilt once the translations are loaded and again whenever the language is switched, outside a frame. Chinese uses Dear ImGui's 2500 common simplified characters plus everything the translation contains rather than the full 21000: at the wheel's 64 px rasterisation the full set would need an atlas taller than Direct3D allows.
- The driving tool gained `op=font`: the face and merged face in use, the folder and game scripts, the glyph count, the atlas size and one probe glyph per script (kana, hangul, hanzi, Cyrillic, Thai), with `lang=` to switch language first - so the fix is proved by reading the atlas, not by squinting at a capture.

## 1.2.6 - 2026-09-16 - untested

### Changed
- **Disable Vanilla Favorites Menu ships ON** (the owner, 2026-09-16: *"wheeler should have a toggle to turn off favorites menu"* - the toggle existed on Wheeler Controls since 1.1.5 but shipped off, so from the player's side there was no such thing). The wheel takes the place of the Favorites menu in this fork, and a vanilla Favorites control that is still live also keeps its buttons: the game refused to give D-pad up to Toggle POV while Favorites held it. `DisableVanillaFavoritesMenu` now defaults to true in the compiled default, in `Controls.defaults.ini` and on the settings page; anyone who wants the vanilla menu back switches it off there. No other behaviour changes.

## 1.2.5 - 2026-09-16 - untested

### Fixed
- **The D-pad moves the tab bar the NAV CURSOR is on, including a bar nested inside another** (the owner, 2026-09-16: *"wherever the nav box is should be considered to be active for navigation because otherwise you have to click every single row and even after clicking it still falls back to that top tab navigation"*). A section can draw a bar inside a bar - Wheeler Controls, then Input Bindings, then Mouse and Keyboard - and only the outermost was being declared, so the nested ones never moved and the press always went back to the top bar. Every bar now registers itself as it draws, and the section picks the one the cursor is actually on; when it is on none of them the DEEPEST bar drawn wins, which is the one the player opened their way into. Focus is only known once a tab item has been submitted, so a bar is judged on what was seen last frame and the request applied on the next - the same one-frame handover the framework uses for its own bar, which is what keeps a press, a mouse click and the tab-list popup from fighting over what is open.
- **A wheel key sharing a button with another Wheeler binding is reported at load** (the owner, 2026-09-16, choosing this over leaving it: *"do 1"*). The settings page refuses a colliding rebind - it is what reported that RT was already Next Wheel - but that check lives in the page, and the wheel key can now be set from OUTSIDE it: Unbind Vanilla Controls writes it into this file from the game's own Controls page, which never passes through the page. A collision could be written in and the wheel would quietly answer two purposes. Nothing is changed automatically - the binding a player set is theirs, and guessing which to drop would be worse than the collision - but it is named in the log, so "the wheel key also does something else" is one search away rather than a mystery.
- **The D-pad walks this mod's OWN tabs inside a section, not just the framework's page tabs** (the owner, 2026-09-16: *"the nav box behaves properly on the main tabs of the mod page for Wheeler, but when going to the other tabs within those tabs, it does not"*). The framework can only see the tab bar IT submits, so the bar each section draws for itself was invisible to navigation and left/right did nothing there. AMF 1.8.7 added `AMF_DeclareInnerTabs(count, current)` for exactly this case; each section now declares its own bar at the start of its frame and applies the tab the D-pad asked for, for one frame only, so a press, a mouse click and the tab-list popup never fight over what is open. Which tab is open is remembered per section, because the framework has to be told before the tab items are submitted. A framework without that export returns nothing and the bar behaves exactly as it did, so this is safe on an older AMF.
- **A refused bind is visible on the wheel, not just in the log** (the owner, 2026-09-16: *"refused wheel bind should give feedback"*). Both refusals added in 1.2.4 - the slot already holds that item, and the slot is full - returned silently, so a full slot was indistinguishable from a dead button; the owner pressed bind four times over before the log explained why nothing happened. The entry is now nudged with the same interpolator an activation bumps, at HALF the scale, so it reads as "heard you, did nothing" rather than as a confirmation. There is no error sound to use here: that feedback was removed for CommonLibSSE-NG compatibility.

## 1.2.4 - 2026-09-16 - untested

### Fixed
- **The items-per-slot limit is enforced when you bind, not silently when the wheel loads** (the owner, 2026-09-16: *"wheeler default should be set to 5 items per slot"* - it already was, everywhere, which is what led to finding this). `MaxItemsPerSlot` defaults to 5 in the compiled settings, the settings page and the shipped INI, but the only place it was applied was deserialisation, where a slot over the limit is truncated with "excess items will be lost". So a sixth item could be added quite happily in edit mode and then disappear at the next game load. A bind that would take a slot past the limit is now refused outright and says so. The check sits after the duplicate check on purpose: re-binding something a full slot already holds still works, because that path only moves the selection and adds nothing.
- **A slot cannot hold the same item twice** (the owner, 2026-09-16, after the cross-slot move was confirmed working: *"you can also add duplicate items to the same slot and just add more entries. So you can have like four different entries on the same slot of the same sword which is a gap we need to fix"*). The move above sweeps OTHER slots, so binding the same item to the slot it was already in kept stacking copies. Binding an item a slot already holds now selects the existing one instead of adding another. It still counts as a bind, so other slots give up their copies as before. Two genuinely different instances are unaffected: a second sword carries its own unique id - the same distinction the move uses - so a differently enchanted or tempered duplicate of the same base form can still sit beside the first, which is what the insert-after-selected behaviour was there to allow.
- **Binding an item to a second slot MOVES it instead of leaving a copy behind** (the owner, 2026-09-16: *"whenever you bind a item to a Wheeler slot and then you bind it to another slot, that it moves the binding from one slot to the other instead of duplicating it"*). Binding in edit mode added the item to the slot under the cursor and did nothing about the slot that already held it, so the same item ended up in both and had to be deleted from the old one by hand. The bind now reports which item it added, and every OTHER slot on that wheel drops its copies of it. Two items of the same base form are still told apart: weapons and armour carry a unique id, because two of the same sword can be enchanted or tempered differently, so only a genuine match is swept. Only the CURRENT wheel is touched - the same item on another wheel is far more likely to be deliberate, since keeping separate sets is what several wheels are for.

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