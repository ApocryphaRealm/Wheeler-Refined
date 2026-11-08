#pragma once

// FAVORITES GO TO DESIGNATED SLOTS (1.3.0; the owner on the Nexus page, 2026-09-17: "a toggle that
// adds a new system that allows you to set what each slot holds and when you favorite an item it
// goes to the designated slot immediately which makes it more in line with being a replacement for
// vanilla favorites menu", and "make this its own sub tab under wheeler behavior tab").
//
// HOW IT WORKS
//   - Two wheels ship on install (the owner: "a second separate wheel that's built in as already
//     made like the current one on install and give it the same treatment but for the magic menu
//     and its skyui categories"). GEOMETRY FIRST: slot 1 (index 0) is drawn at the BOTTOM of the wheel
//     (Wheel.cpp centres entry i at arcSpan*i + pi/2 with screen y downward) and the indices run
//     clockwise from there, so the TOP slot is index n/2. The INVENTORY WHEEL: ten slots - Powers
//     (bottom, locked), Melee Weapons, Ranged Weapons, Apparel, Potions, Shouts (top, locked),
//     Scrolls, Food, Ingredients, Misc (torches and other equippable misc items) ("powers go to the
//     bottom of the wheel visually and shouts go to the top visually"; "ranged weapons should get
//     their own slot separate from melee and ammo wheel makes ammo getting a slot irrelevant"; "misc
//     category gets a slot for equippable items like the torch"). The two locked slots take their
//     category whatever the dropdown for that position says, and pick-up/drop leaves them in place.
//     The MAGIC WHEEL: five slots, the schools in SkyUI order from the bottom clockwise - Alteration,
//     Illusion, Destruction, Conjuration, Restoration. The wheels are found by their ROLE
//     (Wheel::GetRole, "inventory" / "magic"), not their position in the order, their names are drawn
//     under the wheel indicator, and a wheel the Magic or Inventory menu switched to is put back when
//     the wheel closes, so gameplay opens on the wheel the player had ("the magic wheel should be the
//     second wheel not the first wheel"). Every other slot position of both wheels has its category on
//     the Wheel Behavior > Favorites Menu tab; Worn Armor, Ammo, Weapons (both kinds), Spells (any
//     school), Shouts and Powers (together) and Any are there for players who want them.
//   - Apparel EXCLUDES the four main worn pieces (head - biped 30, 31 or a helmet's 31+42 -, body, hands, feet; the owner: "there are
//     armor category items that aren't worn on those slots like the shield and lanterns from other
//     mods"); those are the separate Worn Armor category, assigned to no slot by default.
//   - While an item menu is open (inventory, magic, container, barter) the player's favourites are
//     read every few frames: items through InventoryEntryData::IsFavorited, spells and shouts
//     through MagicFavorites. A form that is favourited now and was not last time is NEW: it is
//     classified, the wheels are searched in order for the first slot whose category takes it
//     (an exact match first, then a slot marked Any), and the HUD says where it went.
//   - 1.7.0 (a player's report; the owner: "If true, then we should fix it"): UN-FAVOURITING TAKES IT OFF THE
//     WHEEL - only while Config::SlotAssignments::RemoveOnUnfavorite is on (1.7.0, default OFF; the owner: "For
//     compatibility ... should be a toggle and not a default behavior"). Off, nothing is removed. Each wheel item
//     remembers who put it there (WheelItem::Source, saved with the wheels). A form that was a
//     favourite at the last scan, is not one now (no other copy of it is either) and is still carried - or is a
//     spell or shout - is taken out of every slot holding it as Source::Favourite; the slot stays. An item placed by
//     hand is never removed, even one later favourited and un-favourited (the add then said "already on the
//     wheel" and left it the player's). An item that stopped being a favourite because it was sold, dropped or used
//     up is left to the inventory prune and Keep Missing. Wheels from a save before 1.7.0 carry no source: the
//     first scan after the load counts an item whose form is a favourite then as the favourites system's, anything
//     else as placed by hand. The Favorites menu is read too, as an item can be un-favourited there.
//   - 1.5.0: two switches at the top of the Favorites Menu tab choose how the Inventory Wheel fills
//     (Config::SlotAssignments): SkyUI Categories (default on) is the search above; By Type (default off)
//     is described at SlotType below. The page lets one be on at a time. With both off, favouriting adds
//     nothing to the Inventory Wheel, the locked Powers and Shouts slots included. The Magic Wheel is
//     searched the same way in all three.
//   - The first read after a game loads only takes the snapshot, so favourites the player already
//     had do not pour onto the wheels at once; only what is favourited from then on moves.
//   - The settings live in wheelBehavior.ini, which the presets on the General tab copy whole, so
//     a preset saves and restores the assignments with everything else.
//   - EVERYTHING here is gated on Config::Control::Wheel::FavoritesSystem, the "Perfected Wheeler
//     Favorites System" toggle on the General tab (Controls.ini, default on): off, no scan runs, the
//     install makes the one wheel it always did, no slot is locked, no wheel name is drawn and the menu
//     does not choose the wheel (the owner: "so it doesn't bleed into the non favorites wheeler").
//
// The scan runs on the game thread through the SKSE task queue (inventory and Scaleform are not
// safe from the render thread that calls Wheeler::Update).
#include <cstdint>
#include <string>

namespace FavoritesToSlots
{
	// Slot categories, in the order the settings-page dropdown lists them.
	enum class Category : int
	{
		Unassigned = 0,
		Any,
		Weapons,
		Apparel,          // shields, jewellery, cloaks, lanterns - apparel NOT on the head/body/hands/feet slots
		WornArmor,        // the four main pieces: head, body, hands, feet
		Potions,          // potions and poisons, as the Potions tab holds both
		Scrolls,
		Food,
		Ingredients,
		Spells,           // any school
		Powers,
		Shouts,
		Ammo,
		Misc,             // torches and other equippable misc items (Light and Misc forms)
		Alteration,       // the magic menu's schools
		Illusion,
		Destruction,
		Conjuration,
		Restoration,
		ShoutsAndPowers,  // one slot for both, for players who want them together
		MeleeWeapons,
		RangedWeapons,    // bows, crossbows, staffs
		Count
	};
	constexpr int kWheelCount = 2;   // wheels that carry assignments: 1 = inventory, 2 = magic
	constexpr int kSlotCount = 10;   // slot positions per wheel that can carry an assignment
	// The shipped defaults, index 0 at the bottom then clockwise. Inventory wheel (ten slots): Powers (bottom,
	// locked), Melee Weapons, Ranged Weapons, Apparel, Potions, Shouts (top, locked), Scrolls, Food, Ingredients,
	// Misc. Magic wheel (five slots): Alteration, Illusion, Destruction, Conjuration, Restoration.
	constexpr int kDefaultCategory[kWheelCount][kSlotCount] = {
		{ 10, 20, 21, 3, 5, 11, 6, 7, 8, 13 },
		{ 14, 15, 16, 17, 18, 0, 0, 0, 0, 0 },
	};
	constexpr int kDefaultSlots[kWheelCount] = { 10, 5 };
	constexpr const char* kRole[kWheelCount] = { "inventory", "magic" };

	const char* CategoryName(Category a_c);
	Category Classify(RE::TESForm* a_form);
	// Whether a slot of category a_slot takes an item of category a_item: the same category, a
	// Spells slot for any school, a Shouts-and-Powers slot for either, an Any slot for anything.
	bool Takes(Category a_slot, Category a_item);
	// The category a slot position of a wheel actually takes: the setting, except the inventory wheel's two
	// locked slots - the bottom (index 0) is always Powers, the top (index count/2) always Shouts.
	Category EffectiveCategory(int a_table, int a_slot, int a_slotCount);

	// 1.4.1 WEAPON TYPE SLOTS, 1.5.0 BY TYPE (Config::SlotAssignments::WeaponTypeSlots, wheelBehavior.ini, default off;
	// only while the favorites system is on). The owner, 2026-10-20: "each slot on the inventory wheel is only keyed to a
	// single weapons type and the wheel is only the weapons category so favoriting an item in the weapons tab will send
	// axes to the same slot and swords to a different slot". Each unlocked position has a type
	// (Config::SlotAssignments::Type, keys W1Slot{n}Type; Auto by default):
	//   - a weapon, potion or poison goes to a slot designated for exactly its type; no room there (Max Items Per Slot)
	//     and it is not added and the slot full text shows;
	//   - a weapon with no designated slot takes the Oblivion wheel's method across AUTO slots only: the first holding a
	//     weapon of the SAME type with room, else the first EMPTY one, else nowhere (slot full text). With no Auto slot at
	//     all it is not added (logged). Potions and poisons never use Auto slots;
	//   - the two locked slots (Powers bottom, Shouts top) stay locked and are skipped. Other inventory favourites
	//     (apparel, shields, food, scrolls, ingredients, misc, ammo) are not added; spells, powers and shouts route
	//     exactly as before, except that on the inventory wheel only the locked slots take them.
	constexpr std::uint32_t kWeaponKeyWarhammer = 0x200u;
	// What a position takes in By Type mode, in the order the settings-page dropdown lists them.
	enum class SlotType : int
	{
		Auto = 0,
		Sword,
		Dagger,
		WarAxe,
		Mace,
		Greatsword,
		Battleaxe,
		Warhammer,
		Bow,
		Crossbow,
		Staff,
		HandToHand,
		Potions,
		Poisons,
		Count
	};
	// The type a form can be designated by; Auto for a form no position can be designated for (food, apparel, ...).
	SlotType SlotTypeOf(RE::TESForm* a_form);
	const char* SlotTypeName(SlotType a_type);
	// True for a category that comes from the inventory side (everything but spells, schools, powers and shouts).
	bool IsInventorySide(Category a_c);
	// The weapon type a form is grouped by: 0x100 + RE::WEAPON_TYPE, or kWeaponKeyWarhammer for a two-handed axe
	// carrying the WeapTypeWarhammer keyword; 0 for anything that is not a weapon.
	std::uint32_t WeaponTypeKey(RE::TESForm* a_form);
	const char* WeaponTypeName(std::uint32_t a_key);

	// Called every frame from Wheeler::Update (render thread). Cheap when the feature is off or no
	// item menu is open; otherwise queues a scan on the game thread every few frames.
	void Tick();

	// Driving-tool entry: queue one scan now regardless of menus, and report the state.
	void ScanNow();
	// 1.7.0: kNewGame / kPostLoadGame - forget the last save's snapshot and take this one's now.
	void OnGameLoaded();
	// Driving-tool entry, test only: un-favourite one form through the game's own InventoryChanges::RemoveFavorite /
	// MagicFavorites::RemoveFavorite (a_formID 0 = the last item the favourites system added), then scan.
	void UnmarkForTest(RE::FormID a_formID);
	// Driving-tool entry, test only (1.7.0): dump what the inventory read sees for one form beside the game's own count;
	// logged and returned as "inspect" in the favorites state.
	void InspectForTest(RE::FormID a_formID);
	// Driving-tool entry, test only: favourite up to a_count un-favourited inventory items and up to a_count of the
	// player's spells and shouts through the game's own InventoryChanges::SetFavorite / MagicFavorites::SetFavorite,
	// on the game thread - what a proof uses instead of pressing F in a menu the tools cannot reach.
	void MarkForTest(int a_count, RE::FormID a_formID = 0);
	void AdoptAllForTest();
	std::string StatusJson();
}
