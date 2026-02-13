# Active Context
## Current work focus (Feb 2026 - Cross-Plugin Input Broker + FavWheel Pause Passthrough)
- **COMPLETED** Implemented Option A input arbitration between Wheeler Refined (`wheeler.dll`), internal AmmoWheel, and external FavWheel.
- **Implementation**:
  1. Added `InputBroker` module (`src/bin/InputBroker.h/.cpp`) with key reservations, priority winner selection, and active owner gating.
  2. Exposed optional broker API through WheelerAPI via `GetInputBrokerAPI(uint32_t)` and `WheelerInputBrokerAPI` function table (versioned, backward compatible).
  3. Integrated broker checks in Wheeler input dispatch (`src/bin/UserInput/Input.cpp`) so blocked keys are not consumed and event chain remains intact.
  4. Integrated ownership/reservation sync in MainWheel and AmmoWheel open/close paths (`src/bin/Wheeler/Wheeler.cpp`, `src/bin/Wheeler/AmmoWheel.cpp`).
  5. Added broker config keys in `wheelBehavior.ini`: `[InputBroker] Enabled`, `DebugLog`, `Priority_MainWheel`, `Priority_AmmoWheel`.
  6. FavWheel integration (`C:\workBench\WheelerFav\wheelerRefinedFavAddon`):
     - Resolves broker API dynamically from `wheeler.dll`.
     - Registers toggle and optional favorites/Q reservations.
     - Acquires/releases active owner while FavWheel is open.
     - Adds pause/system passthrough detection (`UserEvents->pause`) so Start reaches pause menu while FavWheel is open.
     - Suppresses global `dispatchExclusive` nuke when pause passthrough is observed, and disables it entirely while broker is available.
- **Build verified**:
  - `cmake --build build --config Release --target wheeler -- /p:PostBuildEventUseInBuild=false`
  - `cmake --build build --config Release --target favwheel -- /p:PostBuildEventUseInBuild=false` (FavWheel repo)
- **Runtime verification status**: in-game matrix still pending; compile-time verification complete.
- **Hotfix (Feb 2026, post-initial test)**:
  - Adjusted `InputBroker::ShouldProcessKey` so the current `activeOwner` always passes its own key processing before reservation checks.
  - Reason: shared nav keys (e.g., FavWheel category key overlapping AmmoWheel key) were being blocked despite FavWheel owning input.
  - File: `src/bin/InputBroker.cpp`.

## Current work focus (Feb 2026 - Input Compatibility Matrix + InputSpy)
- **COMPLETED** Input routing reliability pass for shared keybinds across MainWheel and AmmoWheel.
- **Implementation**:
  1. `Input.cpp`: Added cooperative compatibility matrix routing order (MainWheel-open -> AmmoWheel-open -> closed-context dispatch) with explicit consume/pass-through outcomes.
  2. `Input.cpp`: Added debug-gated `InputSpy` ring buffer + rate-limited live logs + one-shot dump hotkey output (`logs/input_spy_dump.txt`).
  3. `Input.cpp`: Added LT/RT trigger edge normalization using press/release hysteresis thresholds to stabilize trigger-based binds.
  4. `Controls.cpp`: Toggle dispatch now attempts matched candidates by priority and falls back when a higher-priority toggle candidate does not change wheel state (prevents key starvation on shared binds).
  5. `Controls.cpp`: Added debug binding inventory/conflict matrix logging (`[InputCompat]`) during bind/rebind.
  6. `Config.h/cpp` + `wheelBehavior.ini`: Added `[Debug]` keys `inputSpy`, `inputSpyRateLimitMs`, `inputSpyRingBuffer`, `inputSpyDumpHotkey`.
- **Build verified**: `cmake --build build --config Release --target wheeler -- /p:PostBuildEventUseInBuild=false`.

## Current work focus (Feb 2026 - SlowTimeScale Pause Mode)
- **COMPLETED** SlowTimeScale=0 now triggers vanilla pause via an invisible kPausesGame menu; >0 keeps slow-time with a 0.01 clamp.
- **Implementation**:
  1. Added internal `WheelerPauseMenu` (kPausesGame, pass-through input) with lazy registration.
  2. Added `_wheelerOwnedPauseMenu` ownership tracking + Open/Close helpers for safe cleanup.
  3. Updated `OpenWheeler` branch logic and safety restore to close owned pause menu on stuck states.
- **Notes**: No new INI/dMenu keys; existing slider is now a pause toggle at 0.

## Current work focus (Feb 2026 - Hand Memory / Equipped Hand Memory)
- **COMPLETED** Optional vanilla-style hand memory for restoring left/right hands after leaving a 2H equip state.
- **Implementation**:
  1. `Config::WheelBehavior::HandMemory` keys in `Config.h/cpp` (Enabled, DebugLog, RestoreDelaySeconds, RestoreWindowSeconds, RestoreLeftIfEmpty, RestoreRightIfEmpty).
  2. Runtime state machine in `Wheeler.cpp` polled every frame (captures last non-2H hands, arms restore on exit, respects menu pause and manual equip overrides).
  3. dMenu UI: `Data/SKSE/Plugins/dmenu/customSettings/Wheel Behavior.json` Hand Memory group mapped to `wheelBehavior.ini`.
  4. INI: `Data/SKSE/Plugins/wheeler/wheelBehavior.ini` `[WheelBehavior.HandMemory]` section with defaults.
- **Build verified**: `cmake --build build --config Release --target wheeler -- /p:PostBuildEventUseInBuild=false`.

## Current work focus (Jan 2026 - ActionPolicy Activation System)
- **COMPLETED** Category-driven activation policy system for robust item activation.
- **Problem solved**: Scrolls and other items failing to equip when target hand was occupied.
- **Implementation**:
  1. **ActionPolicy.h/cpp**: New policy system with `ItemCategory` classification, `Action` enum, fallback chains, and post-condition verification.
  2. **WheelItemScroll**: Updated with slot-clearing fallback logic - if hand is occupied, clear it first then equip.
  3. **Debug logging**: `LogActionPolicy` config option in `wheelBehavior.ini` under `[Debug]` section.
  4. **Test plan**: `docs/ActionPolicy_TestPlan.md` with full checklist.
- **Key features**:
  - `Classify(TESForm*)` → ItemCategory (Weapon, Armor, Scroll, Book, Spell, etc.)
  - `GetPolicy(category, hand, intent)` → ordered action chain with fallbacks
  - `VerifyPostCondition()` → checks game state after each action
  - Scroll-specific: toggle behavior + clear-occupied-slot-before-equip
- **Note**: Skyrim scrolls use `RE::ScrollItem` (FormType::Scroll), NOT `TESObjectBOOK`. Classification already correct.
- Build verified: `cmake --build build --config Release --target wheeler`.

## Current work focus (Jan 2026 - TransformWheels werewolf spell collection fix)
- **COMPLETED** Werewolf wheel missing abilities (howls).
- **Key finding**: Werewolf howls (e.g., Howl of Rage, Revert Form) are `TESShout` forms (formType 119) unlocked via player progression/race, NOT `SpellItem`s.
- **Root cause**: The original logic only scanned for `SpellItem`s in `magicEyes` and `ActorBase`.
- **Fix in `TransformWheelManager.cpp`**:
  1. **New `CollectTransformKitShouts`**: Scans all game shouts via `TESDataHandler`, filters by `shout->GetKnown()` (player knowledge) and kit keywords ("Howl", "Werewolf", "Revert").
  2. **Integration**: Merges known kit shouts into the candidate list in `TryPopulateTransformWheel`.
  3. **Verified**: Logs confirm `kitShouts` are detected and populate the wheel correctly. Race-based shout override logic remains correct as a backup.
- Build verified: `cmake --build build --config Release --target wheeler`.

## Current work focus (Jan 2026 - GlobalScale Revert)
- **Objective**: Scale mouse sensitivity proportionally with `GlobalScale` (visual wheel size).
- **Attempts**:
  1.  **Scaled Mouse Delta**: `UpdateCursorPosMouse` delta multiplied by `GlobalScale`.
  2.  **Scaled Cursor Radius**: `getCursorRadiusMax` multiplied by `GlobalScale`.
- **Result**: Both approaches caused the mouse cursor to become trapped or insensitive when `GlobalScale` changed dynamically.
- **Decision**: **Full Revert**. Code returning to `GlobalScale = 1.0` baseline.
  - Removed `GlobalScale` slider from `Wheel Behavior.json`.
  - Removed `GlobalScale` variable from `Config.h` and INI reading.
  - Reverted `UpdateCursorPosMouse` and `getCursorRadiusMax` to original logic.
- **Future Direction**: Investigate scaling `CursorRadiusPerEntry` in `OffsetSizingToViewport` for a synchronized visual/input scale.

## Current work focus (Jan 2026 - MainWheel Hover Revert)
- Removed intent-aware mouse hover model toggle/code; legacy mouse selection remains.
- Restored gamepad hover selection by defaulting to candidate + hysteresis from backup.
- dMenu Wheel Behavior: removed new model toggle, kept mouse hover debug options.

## Current work focus (Jan 2026 - MainWheel Mouse Stabilization Guard Rails)
- Added opt-in mouse guard rails (center hold, jump guard, boundary margin) layered after legacy candidate selection.
- Added optional motion hinting (velocity blend) and mouse stabilization debug rings/logs.
- Updated mouse hover overlay to show stabilization state + guard reason.
- Pending in-game validation with stabilization Enabled on/off.
- Updated guard logic to two-stage final commit with optional step-toward and single consolidated debug log.

## Current work focus (Jan 2026 - Keep Missing Items)
- KeepMissing feature is implemented and in test pass.
- Fix applied for gear restore: when a weapon/armor returns, reacquire via FormID fallback, refresh uniqueID if available, and restore usability without re-adding the slot.

## Current work focus (Dec 2025 - MainWheel Input Capture + DIK Revert)
- Fix MainWheel input capture logging to be authoritative and diagnose open/close edge cases.
- Revert MainWheel to DIK-based input binding only (no rebind UI or overrides), while keeping AmmoWheel rebind intact.

## Completed implementation notes
- **MainWheel debug helper**: `MainWheelDebug.h/.cpp` with category checks and rate-limited logging.
- **Instrumentation**: main wheel open/close/config/input/scaling/clamp/reskin logs + perf timing in `Wheeler.cpp`, plus asset/indicator logs in `Wheel.cpp` and `WheelEntry.cpp`.
- **Rebind wiring**: Mod callback handlers for rebind/cancel/reset actions for AmmoWheel; MainWheel reverted to DIK input only.
- **INI/JSON updates**: `wheelBehavior.ini` (MainWheel.Debug/LowEnd), `AmmoWheel.ini` (ToggleKey*Name), `Wheel Behavior.json` (Low-End/Debug), `Ammo Wheel.json` (Rebind controls + key name fields).
  - **MainWheel keybind rollback**: removed MainWheel rebind UI group and keybind override writes; MainWheel now reads legacy DIK keys only.
  - **MainWheel input capture**: centralized event tracking and decision logging with edges, heldMs, consumer, and open/close reason.

## Current work focus (Dec 2025)
- **AmmoWheel Unified Reskin System Documentation** (COMPLETED):
  - Created comprehensive end-to-end guide for reskin/layout/design system
  - Documented preset resolution chain, config file hierarchy, visual targets
  - Fixed Default preset PopupBubble path to use existing blood_mist flipbook
  - Created CONFIG_MAP.md with code references for all settings
  - Created CONFLICT_MATRIX.md with precedence chains and toggle decisions
  - Created OPTIONAL_PATCH.md explaining popup flipbook root cause
  - Created INI_TEMPLATES with simple/advanced/theme recipes
  - Annotated AmmoWheel_Reskin.ini with per-key documentation
- **Remaining**: Annotate AmmoWheel_Styles.ini, AMMO_KID.ini, AmmoWheel.ini, wheelBehavior.ini, Styles.ini (legacy notes), skins/default/skin.ini (legacy notes)

## Current work focus (Jan 2026 - InstantSpell UI & Focus Fixes)
- **InstantSpell UI & Feedback**:
  - Implemented visual countdown arc for InstantSpell attempts.
  - Added smooth alpha pulse effect when threshold is reached (3.0s).
- **Control & Input Fixes**:
  - **RTU OFF Fix**: Restored vanilla behavior where RMB directly equips to Left Hand (previously was only latching).
  - **Hand Resolution**: Decoupled hand selection from RTU timing via `ResolveTargetHand()`.
  - **Cancel Logic**: Removed MMB binding (conflict with legacy scroll cancel); maintained `CancelInstantSpell()` internally.
- **Polish & Stability**:
  - **State-Change Logging**: Added gated logging for InstantSpell entry/ready/cancel events (no frame spam).
  - **Context Reset**: Verified cancel suppression resets on entry change and wheel close.

## Current work focus (Jan 2026 - Safe Debug/Guard Patch)
- **Debug config**: Added `[Debug]` section in `wheelBehavior.ini` and dMenu with `LogActivateRejects`, `LogMenuBlockReasons`, `LogPopupAnim`, `PopupAnimLogIntervalMs`.
- **Activation guard**: Main wheel activation now checks runtime `FormID` via `RE::TESForm::LookupByID` and skips invalid activations with optional debug log.
- **Menu block diagnostics**: Added gated log line to report conflicting menu name and key flags when `DeniedMenuBlocked` occurs.
- **Popup animation logs**: AmmoWheel popup animation logging is gated + rate-limited (no per-frame spam).

## Current work focus (Jan 2026 - Ammo Limit)
- **Objective**: Add configurable `ArrowLimit` and `BoltLimit` to truncate the sorted list (top N items).
- **Implementation**:
  - `Config.h/cpp`: Added `Sort::ArrowLimit` and `Sort::BoltLimit` (clamped 0-128).
  - `AmmoWheel.cpp`: Added single-pass keep-in-order limiter in `RefreshAmmoList` (preserves existing sort).
  - `AmmoWheel.ini`: Added `[Sort]` keys `ArrowLimit`/`BoltLimit`.
  - `Ammo Wheel.json`: Added "Sort Limits" group with sliders.
- **Verification**: Built and verified compilation; added logging for manual verification.


- AmmoWheel refactor: make AmmoWheel an independent, modern ammo selector with stable navigation and readable UI.
- AmmoWheel performance tier pass (Low/Balanced/High) via config-only overrides; no reskin or indicator code changes.
- AmmoWheel stability: prevent CTDs (hard CTD w/out crash log) and eliminate repeated equip spam by debouncing activation.
- dMenu/INI integration for AmmoWheel: ensure dMenu JSON INI keys match Config parsing and keep backward compatibility.
- Stability and crash prevention in serialization/deserialization paths.
- Privacy: ensure no absolute source paths leak into logs or binaries.
- Keep the Wheel Behavior feature set cleanly separated from legacy config naming and expose user-friendly controls via dMenu.
- Integrate mod-driven “use” behaviors safely (e.g., Skyrim’s Got Talent instruments) without synthetic input.
- AmmoWheel UI/UX polish tasks:
  - Unified input filtering for mouse and gamepad.
  - Center panel positioning with viewport clamping.
  - Slot label rendering with multi-line word wrapping.
  - Popup bubble with smooth scale/fade animation.
  - Fixed-open mode LMB/RMB handling.
  - Visual regression fixes (opacity, border rings, theme interactions).
  - dMenu settings reorganization for better UX.

## AmmoWheel refactor (Dec 2025)
- **Goal**: independent, modern ammo selector wheel (mouse + gamepad) with predictable hover, no "center-pull", and configurable UI.
- **State** (AmmoWheel owns its hover/selection):
  - `_hoveredIndex`, `_hoveredTime`
  - `_lastSelectedIndex` (fallback continuity)
  - `_lastSelectedAmmoID` (FormID-based restoration)
  - `_cursorPos` (relative cursor)
  - `_hoverPopupScale` (popup animation; forced to 1.0 on open)
  - `_activationConsumed` (debounce flag to prevent repeated equip spam)
  - `_mouseFilter`, `_gamepadFilter` (CursorFilter structs for unified input filtering)
- **Open behavior**:
  - `TryOpen()` refreshes list, sets `Opening`, resets timers, and chooses `_hoveredIndex` via `FindInitialHoverIndex()`.
  - Cursor is initialized to point at the initial slot (prevents first-frame jitter/center behavior).
  - Popup is shown immediately on open (`_hoverPopupScale = 1.0f`).
- **Navigation / hover acquisition**:
  - `getHoveredIndex()` clamps to nearest slot instead of returning -1, preventing selection drop.
  - **TASK 1**: Unified `CursorFilter` struct applies deadzone + exponential smoothing for both mouse and gamepad.
  - Config: `MouseDeadzone`, `MouseSmoothingSpeed`, `GamepadDeadzone`, `GamepadSmoothingSpeed`.
- **Activation (equip)**:
  - Activation route is via RTU logic on `Close()` (when `UseRTUSystem` enabled and `_hoveredTime >= RTUHoverDelay`).
  - `ActivateHoveredAmmo()` is hardened:
    - Debounced by `_activationConsumed`
    - Skips redundant equip when hovered ammo already equipped
    - Null guards for entry/player/equipManager
  - `_activationConsumed` is reset on `TryOpen()` and `ForceClose()`.
  - **TASK 4**: `HandleMouseButton()` for fixed-open mode LMB/RMB handling (consumes input to prevent bow firing).
- **Time slow while open**:
  - Optional `[TimeSlow]` config for AmmoWheel (`Enabled`, `SlowTimeScale`), default OFF.
  - dMenu "Time Slow" group; preset overrides write to `AmmoWheel_Override.ini`.
  - Uses same timescale guard as Main Wheel: only apply when current timescale ~1.0; restore only if AmmoWheel modified (avoids Slow Time shout conflict).
- **Layout scaling & resolution mapping (new)**:
  - Unified `AmmoWheel::LayoutScaling` now reuses `MainWheel.Layout.ini`/`user/MainWheel.Layout.ini`.
  - Geometry/popup/style px values (radius, paddings, indicator thickness) scale by the combined layout+resolution multiplier while normalized position anchors remain untouched.
  - Missing `[AmmoWheel.LayoutScaling]` sections in the user override are repaired on demand so scaling activates automatically once the shared file exists.
  - Open-time logging and GameSpace clamping ensure the wheel stays visible across resolution switches and mismatch cases.
- **Layout scaling maintenance (Dec 2025)**:
  - Shared layout file now doubles as both template and user override for Main and Ammo layout state; the template carries safe defaults for both sections, and the user override is auto-created once (Ref=Display size) so first run has no visual change.
  - If a user override is missing `[AmmoWheel.LayoutScaling]`, it is appended automatically, logged as `Repaired user override`, and reloaded so subsequent logs report `src=user` and `geom=on`.
  - Combined layout+mismatch scales are now used consistently for geometry/popup/style px values while clamp adjustments log before/after centers in GameSpace.
- **Performance tiers (Dec 2025)**:
  - Added config-only Low/Balanced/High tiers with optional override flags per category (visual polish, animations, popup, labels, center, indicators).
  - New `[Performance]` section in AmmoWheel.ini and dMenu "Performance" group; overrides apply post-load without touching reskin assets.
- **Center panel positioning (TASK 2)**:
  - `calculateCenterPanelPosition()` biases panel toward arc midpoint for partial arcs.
  - Viewport clamping with configurable safe margin.
  - Config: `CenterPanelInsetRatio`, `CenterPanelSafeMargin`.
- **Slot label rendering (TASK 3)**:
  - `wrapTextForSlot()` provides multi-line word wrapping.
  - Dynamic layout mode based on slot angular position (1-3 lines).
  - Per-slot clipping to prevent label overlap.
  - Config: `LabelMultiLine`, `LabelMaxSlotArcRatio`.
- **Popup bubble (TASK 5)**:
  - Circular bubble with fixed radius, magnified icon, wrapped text.
  - Smooth scale/fade animation with easing.
  - Viewport clamping for on-screen placement.
  - Config: `PopupBubbleRadius`, `PopupCircular`, `PopupAnimationSpeed`.
- **dMenu integration**:
  - Panel: `Data/SKSE/Plugins/dmenu/customSettings/Ammo Wheel.json`
  - INI: `Data/SKSE/Plugins/wheeler/AmmoWheel.ini`
  - Config parsing reads both prefixed keys (as written by dMenu JSON) and legacy keys.
- **Known issue addressed**:
  - CTD during ammo selection with log spam: repeated "AmmoWheel: Equipped ammo ...".
  - Fix: debounce + already-equipped suppression in `ActivateHoveredAmmo()`.

## Current Wheel Behavior (RTU) design
- **Normal**: `ReleaseToUse = false` - original Wheeler behavior (no activation on wheel close).
- **Release-to-Use (RTU)**: `ReleaseToUse = true` - on wheel close, activate hovered slot.
  - Per-category toggles (only applied when RTU is enabled):
    - `RTUAlchemy`, `RTUSpell`, `RTUShout`
- **CloseWheelAfterUse**: optional; closes the wheel after activation while open (e.g. click/hover-activation).
- **Instant Spell (cheaty)**: `InstantSpell = true` (only meaningful when RTU + RTUSpell are enabled)
  - Casts immediately (bypasses charge time) when possible; otherwise falls back to equip behavior.

## Cooldown timer text styling
- Cooldown timer text (shown at slot center when `Cooldowns.Enabled` + `Cooldowns.ShowTimer`) is configurable.
- Config section: `[Cooldowns.TimerText]` in `Data/SKSE/Plugins/wheeler/wheelBehavior.ini`
  - `FontIndex` (0 = current UI font, 1 = default font atlas index 0)
  - `Size` (pixels)
  - `Color` (UInt32 ARGB)
- dMenu exposes these under **Cooldown Overlays -> Timer Text Style** in `wheeler-dev/Data/SKSE/Plugins/dmenu/customSettings/Wheel Behavior.json`.

## Key implementation points
- RTU activation routing is implemented in `wheeler-dev/src/bin/Wheeler/Wheeler.cpp` and gates behavior by hovered `WheelItem*` type + config toggles.
- Config is loaded from `Data/SKSE/Plugins/wheeler/wheelBehavior.ini` under `[WheelBehavior]`.
- Backward compatibility:
  - Legacy `InstantUse.ini` and legacy `[InstantUse]` keys are still supported.
  - Migration writes `wheelBehavior.ini` if only the legacy file exists.
  - Upgrade safeguard: if `wheelBehavior.ini` exists but is still defaults and legacy is non-default, legacy settings are migrated into `wheelBehavior.ini` automatically.

## Consumables depletion behavior
- `ClearDepletedConsumables = true`: when an alchemy item reaches 0, Wheeler clears the slot safely after the wheel fully closes (prevents stale "0" slots and avoids historical last-consumable CTDs).
- Crafted/dynamic potions:
  - `ClearDepletedConsumables = true`: do not show `AlchemyDynamicIDConsumptionWarning`; allow consuming the last crafted potion and then clear the slot safely.
  - `ClearDepletedConsumables = false`: keep legacy safeguard behavior (warning blocks consuming the last crafted potion).

## dMenu UI
- Panel: `wheeler-dev/Data/SKSE/Plugins/dmenu/customSettings/Wheel Behavior.json`
- Writes to: `wheeler-dev/Data/SKSE/Plugins/wheeler/wheelBehavior.ini`
- Exposes:
  - RTU on/off + per-category toggles
  - Hover delay slider (RTU only)
  - CloseWheelAfterUse toggle (RTU only)
  - ClearDepletedConsumables (removes 0-count consumables from slots safely)
  - AutoDrawOnUse
  - InstantSpell toggle (RTU + RTUSpell only)
  - Hover delay indicator styling (Enabled, Radius, RadiusOffset, Thickness, Color, BackgroundColor)
  - Cooldown overlay controls (Enabled, ShowTimer, ContentDimAlpha, SelectedIndicator*, TimerText style)
  - Restore Defaults button (restores to first saved Wheel Behavior config)

## Skyrim’s Got Talent (SGT) instruments integration
- Problem: equipping/using instrument `MISC` items via `RE::ActorEquipManager::EquipObject` does not reliably trigger the same Papyrus/OAR animation behavior as using the item in the inventory menu.
- Solution implemented:
  - Detect instrument type (Flute/Drum/Lute) in `WheelItemMisc`.
  - Resolve the corresponding SGT spell from `SkyrimsGotTalent-Bards.esp` (`_Talent_FluteSpell`, `_Talent_DrumSpell`, `_Talent_LuteSpell`).
  - Queue the spell to be applied after the wheel fully closes.
  - In `ProcessPendingActions`, **AddSpell** to the player (required for OAR `HasSpell` conditions) and then cast via `CastSpellImmediate`.
- Status: working in-game; animations now match OAR replacer conditions.

## RTU Anti-Slip
- **v1 (current)**: Simple deadzone + lockout + dwell logic. Stable and predictable.
- **v2 (reverted)**: Added stable hover tracking and directional hysteresis. Caused issues with hover acquisition and was rolled back.
- Anti-slip state variables: `_antiSlipLockUntil`, `_stableHoverIdx`, `_lastHoverIdx`, `_hoverDwellStart`.

## Privacy Build Configuration
- **Goal**: No absolute source paths, usernames, or local folder structures in logs, crash reports, or binaries.
- **Implementation**:
  - `SPDLOG_NO_SOURCE_LOC` defined in `PCH.h` and CMake to disable spdlog source location injection.
  - Log pattern changed from `"%s(%#): [%^%l%$] %v"` to `"[%^%l%$] %v"` (no file/line).
  - MSVC `/d1trimfile:${CMAKE_SOURCE_DIR}/` flag strips project root from `__FILE__` macros.
- **Result**: User paths (`C:\Users\...`) no longer appear in the DLL. Only external library paths (CommonLibSSE) remain.

## Serialization Crash Fix (Last Consumed Poison CTD)
- **Problem**: CTD on save reload after consuming the last instance of certain poison items (e.g., Apothecary "Lingering Poison").
- **Root cause**: `WheelItemAlchemy` constructor could crash if `GetCostliestEffectItem()` returned null.
- **Fix**:
  - Added null guard in `WheelItemAlchemy` constructor for both the alchemy item and its costliest effect.
  - Hardened `WheelItemFactory::MakeWheelItemFromJsonObject` with diagnostic logging and type-safe lookups.
  - Added try-catch per entry/item in `Wheel::SerializeFromJsonObj` and `WheelEntry::SerializeFromJsonObj`.
  - Added global exception handling in `SerializationEntry::Load` - clears wheel state on any failure.
  - Fixed off-by-one in `selecteditem` bounds checking.

## Current Work Focus (Feb 2026 - Vanilla Power Activation Pipeline)
- Goal: instant-cast powers/greater powers must use vanilla shout/power activation so once-per-day cooldown and mod scripts/events work correctly.
- Added `TryActivateEquippedShoutOrPowerVanilla(...)` helper in `src/bin/Wheeler/Wheeler.cpp` that uses `PlayerControls::shoutHandler` with synthetic shout key press/release.
- Added deferred power queue: `QueuePowerActivation(FormID)` + `_pendingPowerFormID`.
- Updated RTU and Hold-to-Use instant-cast branches:
  - For power spell types (`kPower`, `kLesserPower`, `kVoicePower`), queue deferred power activation instead of `WheelItemSpell::CastImmediate`.
  - Non-power spell path is unchanged.
- Deferred execution in `ProcessPendingActions()`:
  - Waits for wheel closed + popup clear.
  - Equips power to voice slot if needed.
  - Schedules next-task vanilla activation call (safer than same-frame equip+activate).
- Added focused `[PowerPipe]` debug logs for queue/equip/task/activate success/failure.
- Build check passed: `cmake --build build --config Release --target wheeler`.

## Feb 2026 - Scripted Misc Dispatch Fix (LotD repeat activation)
- Implemented `ScriptedMiscDispatchMode` for deferred scripted MISC activation after wheel close.
- Auto mode defaults to single-dispatch via `EquipObject` and routes Yps/Shovel items to temp-ref `OnEquipped`.
- Added concise `ScriptedMiscUse` log line with formId, uniqueId, editorID, mode, and auto reason.
- Added a 200ms dedupe guard to avoid accidental double-execution of the same pending misc activation.
- Config key: `[WheelBehavior] ScriptedMiscDispatchMode` (default 0) in `Data/SKSE/Plugins/wheeler/wheelBehavior.ini`.
- Ported the same fix to FavWheel (`C:\workBench\WheelerFav\wheelerRefinedFavAddon`) with matching config key in `Data/SKSE/Plugins/favwheel/wheelBehavior.ini`.
