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
//     (an exact match first, then a slot marked Any), and the HUD says where it went. Nothing is
//     taken off a wheel when a favourite is removed - what the player put there stays theirs.
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

	// Called every frame from Wheeler::Update (render thread). Cheap when the feature is off or no
	// item menu is open; otherwise queues a scan on the game thread every few frames.
	void Tick();

	// Driving-tool entry: queue one scan now regardless of menus, and report the state.
	void ScanNow();
	// Driving-tool entry, test only: favourite up to a_count un-favourited inventory items and up to a_count of the
	// player's spells and shouts through the game's own InventoryChanges::SetFavorite / MagicFavorites::SetFavorite,
	// on the game thread - what a proof uses instead of pressing F in a menu the tools cannot reach.
	void MarkForTest(int a_count);
	void AdoptAllForTest();
	std::string StatusJson();
}
