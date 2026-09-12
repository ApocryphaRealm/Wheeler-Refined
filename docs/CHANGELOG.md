# Changelog

Entries dated 2026-04-26 and earlier are C0kadam's, from the upstream Wheeler Refined releases this
fork derives from. They are kept verbatim: they are his release history and are not ours to restate.
Entries above that line are the ApocryphaRealm fork, which starts its own version line at 1.0.0
rather than continuing his numbering.

## 1.0.0 - 2026-09-12 (ApocryphaRealm fork)

- Settings page: an in-game page built from the eight dMenu descriptors - 980 nodes, 810 controls
  across 8 panels and 86 tabs, with each value resolved from the shipped defaults with the user INI
  layered over it, and the page reporting which layer a value came from.
- Input: the events already passing through `Input::ProcessAndFilter` are translated into Wheeler's
  ImGui context, and keymap capture lets all 63 keymaps be rebound in place. Upstream's
  `Controls::BeginRebind` only ever understood five Ammo Wheel targets.
- New setting `Control.Wheel/SettingsPageKey` (default 87 = F11) opens and closes the settings page.
- New setting `Control.Wheel/DisableVanillaFavoritesMenu` suppresses the game's own Favorites menu
  entirely, distinct from the existing option that only decides whether Wheeler may open over it.
- Wheel slot count is settable at runtime, clamped 1-64, refusing to discard a slot that still
  holds an item.
- Boolean settings render as on/off switches rather than checkboxes.
- A devbench driving tool (`wheeler.page`) exposes status/open/close/toggle/tabs/list/get/set/
  rebind/selecttab so the page can be operated and verified without a person at the keyboard.
- Fixed: `translations.txt` shipped a duplicate key block that warned on every launch and left two
  messages blank; the slot-count strings it never carried are now present.

## 2026-04-26
- Release metadata updated for `v1.3.3`.
- Build/version logging banner updated to report `v1.3.3` with build date `4/26/2026`.
- Windows file metadata/resource version now aligns with the `1.3.3` release.

## 2026-04-19
- Release metadata updated for `v1.3.2`.
- Build/version logging banner updated to report `v1.3.2` with build date `4/19/2026`.
- Windows file metadata/resource version now aligns with the `1.3.2` release.

## 2026-02-15
- Input filtering: while Main Wheeler is open, background menu activation user-events (`Activate`/`Accept`) are now consumed in `InventoryMenu`, `ContainerMenu`, `MagicMenu`, `FavoritesMenu`, `LootMenu`, and `LootMenuCF` to prevent vanilla equip/use behind the wheel.
- Input fallback: if that activation input is not bound to any Wheeler action, Wheeler routes it to confirm down/up as a user-event fallback (no hardcoded gamepad button IDs, no override of user bindings).
- Performance: AmmoWheel visible draw now uses `InventorySnapshotCache` (default 0.25s, clamped 0.05-1.0s) instead of calling `GetInventory()` every frame.
- Performance: AmmoWheel per-entry slot labels are precomputed into a cached layout and no longer run per-frame wrap/measure/truncate loops in `drawSlot()`.
- Performance: AmmoWheel center panel now uses cached damage/max-damage state plus cached field/text layout; per-frame full ammo damage scans and per-frame `CenterFields::Order` parsing were removed.
- Perf diagnostics: added 1Hz debug-gated telemetry (`[Perf][AmmoWheel] ...`) controlled by `AmmoWheel.ini` `[Debug] LogPerf`.

## 2026-02-13
- Performance: MainWheel and FavWheel now use a timed inventory snapshot cache while the wheel popup is visible (default refresh 0.25s, clamped 0.05-1.0s) instead of calling `GetInventory()` every frame.
- Performance: `WheelEntry` render-path missing-state checks now use the passed inventory snapshot (`IsAvailable`) and no longer call `IsInPlayerInventory()` during draw.
- Perf diagnostics: added rate-limited (1Hz) snapshot refresh telemetry under `MainWheelDebug::Perf` showing refresh ms and refresh count per second.
- Stability: snapshot cache is invalidated on wheel close to avoid stale state carryover between sessions.

## 2026-02-10
- Removed per-action modifier bindings. Kept optional modifier only for wheel toggle open/close.
- Toggle modifier keys are non-exclusive and can be reused for other bindings. Toggle only triggers when modifier is held.
- Added cooperative input compatibility routing between MainWheel and AmmoWheel to prevent cross-module key starvation on shared bindings.
- Added debug-gated InputSpy diagnostics (`[Debug] inputSpy*`) with ring buffer + dump hotkey, plus startup binding conflict inventory reporting.
