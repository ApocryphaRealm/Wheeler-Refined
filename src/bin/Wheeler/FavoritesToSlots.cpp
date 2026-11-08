#include "bin/Wheeler/FavoritesToSlots.h"

#include "bin/Config.h"
#include "bin/Texts.h"
#include "bin/Utilities/Utils.h"
#include "bin/Wheeler/WheelItems/WheelItem.h"
#include "bin/Wheeler/WheelItems/WheelItemFactory.h"
#include "bin/Wheeler/Wheeler.h"

#include <atomic>
#include <mutex>
#include <unordered_set>

namespace FavoritesToSlots
{
	namespace
	{
		// A favourite is identified by form id plus, for weapons and armour, the inventory
		// unique id - the same pair the wheel items themselves are made from.
		struct Key
		{
			RE::FormID formID{ 0 };
			std::uint16_t uniqueID{ 0 };
			bool operator==(const Key& o) const { return formID == o.formID && uniqueID == o.uniqueID; }
		};
		struct KeyHash
		{
			std::size_t operator()(const Key& k) const { return std::hash<std::uint64_t>{}((static_cast<std::uint64_t>(k.formID) << 16) | k.uniqueID); }
		};

		std::mutex g_lock;
		std::unordered_set<Key, KeyHash> g_known;   // favourites seen at the last scan
		bool g_seeded = false;                       // first scan of a session only takes the snapshot
		std::atomic<bool> g_scanQueued{ false };
		int g_framesSinceScan = 0;
		bool g_menuWasOpen = false;
		int g_scans = 0, g_added = 0, g_skipped = 0, g_removed = 0;
		RE::FormID g_lastAddedFormID = 0;   // the driving tool's unmark default
		std::string g_lastAction;

		bool ItemMenuOpen()
		{
			auto* ui = RE::UI::GetSingleton();
			// 1.6.1: the Favorites menu too - an item can be un-favourited there.
			return ui && (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::MagicMenu::MENU_NAME) ||
						  ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) || ui->IsMenuOpen(RE::BarterMenu::MENU_NAME) ||
						  ui->IsMenuOpen(RE::FavoritesMenu::MENU_NAME));
		}

		std::uint16_t UniqueIDOf(RE::InventoryEntryData* a_entry)
		{
			if (!a_entry || !a_entry->extraLists) { return 0; }
			for (auto* xl : *a_entry->extraLists) {
				if (!xl) { continue; }
				if (auto* u = xl->GetByType<RE::ExtraUniqueID>()) { return u->uniqueID; }
			}
			return 0;
		}

		// The first (wheel, slot) whose category takes the item: exact matches on the inventory wheel then the
		// magic wheel first, then Any slots. The wheels are found by role; a save from before roles existed
		// falls back to positions 1 and 2.
		int WheelOfTable(int a_table)
		{
			const int byRole = Wheeler::FindWheelIndexByRole(kRole[a_table]);
			return byRole >= 0 ? byRole : a_table;
		}

		// The inventory wheel's two locked positions, exactly where EffectiveCategory locks them.
		bool IsLockedSlot(int a_table, int a_slot, int a_slotCount)
		{
			if (a_table != 0 || a_slot < 0 || a_slot >= kSlotCount) { return false; }
			return a_slot == 0 || (a_slotCount >= 2 && a_slot == a_slotCount / 2);
		}

		bool FindSlot(Category a_item, int& a_wheel, int& a_slot)
		{
			int wheelOf[kWheelCount];
			for (int w = 0; w < kWheelCount; ++w) { wheelOf[w] = WheelOfTable(w); }
			// 1.4.1: with By Type on, the inventory wheel's free positions belong to By Type routing; anything
			// routed here (spells, powers, shouts) may only use its two locked slots there.
			const bool weaponWheel = Config::SlotAssignments::WeaponTypeSlots;
			// 1.5.0: with both switches off, favouriting adds nothing to the inventory wheel.
			const bool skipInventoryWheel = !weaponWheel && !Config::SlotAssignments::SkyUICategories;
			for (int pass = 0; pass < 2; ++pass) {
				for (int w = 0; w < kWheelCount; ++w) {
					if (skipInventoryWheel && w == 0) { continue; }
					const int count = Wheeler::GetSlotCountOfWheel(wheelOf[w]);
					if (count <= 0) { continue; }
					for (int s = 0; s < kSlotCount && s < count; ++s) {
						if (weaponWheel && w == 0 && !IsLockedSlot(w, s, count)) { continue; }
						const auto cat = EffectiveCategory(w, s, count);
						if (pass == 0 ? (cat != Category::Any && Takes(cat, a_item)) : (cat == Category::Any)) { a_wheel = wheelOf[w]; a_slot = s; return true; }
					}
				}
			}
			return false;
		}

		// The type a position of the inventory wheel is set to. A position past the table, or a value no dropdown
		// offers (a hand-edited INI), is Auto.
		SlotType SlotTypeAt(int a_slot)
		{
			if (a_slot < 0 || a_slot >= kSlotCount) { return SlotType::Auto; }
			const int v = Config::SlotAssignments::Type[a_slot];
			return (v > 0 && v < static_cast<int>(SlotType::Count)) ? static_cast<SlotType>(v) : SlotType::Auto;
		}

		enum class ByTypeRoute
		{
			Found,
			Full,     // Auto slots exist, none has room: the slot full text
			NoSlot    // nothing on the wheel can take this item as it is set up: logged only
		};

		// 1.5.0 By Type: a slot designated for exactly this type comes first. When every designated slot is full the first
		// one is still handed back, so AddItemToSlotOnWheel tells "already on the wheel" from "slot full" as it does for
		// every other route. False when no slot is designated for it. a_why says what was decided.
		bool FindDesignatedSlot(SlotType a_type, int a_wheel, int a_count, int& a_slot, std::string& a_why)
		{
			const int cap = static_cast<int>(Config::WheelBehavior::MaxItemsPerSlot);
			int firstDesignated = -1;
			for (int s = 0; s < a_count; ++s) {
				if (IsLockedSlot(0, s, a_count) || SlotTypeAt(s) != a_type) { continue; }
				if (firstDesignated < 0) { firstDesignated = s; }
				const auto held = Wheeler::GetSlotFormIDs(a_wheel, s).size();
				if (static_cast<int>(held) < cap) {
					a_slot = s;
					a_why = "slot designated for " + std::string(SlotTypeName(a_type)) + " (" + std::to_string(held) + "/" + std::to_string(cap) + ")";
					return true;
				}
			}
			if (firstDesignated < 0) { return false; }
			a_slot = firstDesignated;
			a_why = "every slot designated for " + std::string(SlotTypeName(a_type)) + " is full";
			return true;
		}

		// 1.4.1 Weapon Type Slots, 1.5.0 By Type: with no slot designated for this type, the first AUTO slot on the
		// inventory wheel (locked Powers / Shouts slots skipped) that already holds a weapon of this type and has room
		// under Max Items Per Slot, else the first empty Auto one. The type is read from the items in the slot each time,
		// never stored (the Oblivion wheel's way); a slot holding potions reads as key 0, which is no weapon's key, so it
		// is simply occupied. a_key is 0 for potions and poisons, which do not use Auto slots. a_why says what was decided.
		ByTypeRoute FindByTypeSlot(SlotType a_type, std::uint32_t a_key, int& a_wheel, int& a_slot, std::string& a_why)
		{
			const int wheel = WheelOfTable(0);
			const int count = Wheeler::GetSlotCountOfWheel(wheel);
			if (count <= 0) { a_why = "no inventory wheel"; return ByTypeRoute::NoSlot; }
			if (FindDesignatedSlot(a_type, wheel, count, a_slot, a_why)) {
				a_wheel = wheel;
				return ByTypeRoute::Found;
			}
			if (a_key == 0) {
				a_why = "no slot is designated for " + std::string(SlotTypeName(a_type));
				return ByTypeRoute::NoSlot;
			}
			const int cap = static_cast<int>(Config::WheelBehavior::MaxItemsPerSlot);
			int firstEmpty = -1;
			int sameTypeFull = 0;
			int autoSlots = 0;
			for (int s = 0; s < count; ++s) {
				if (IsLockedSlot(0, s, count) || SlotTypeAt(s) != SlotType::Auto) { continue; }
				++autoSlots;
				const auto ids = Wheeler::GetSlotFormIDs(wheel, s);
				if (ids.empty()) {
					if (firstEmpty < 0) { firstEmpty = s; }
					continue;
				}
				bool same = false;
				for (RE::FormID id : ids) {
					if (WeaponTypeKey(id ? RE::TESForm::LookupByID(id) : nullptr) == a_key) { same = true; break; }
				}
				if (!same) { continue; }
				if (static_cast<int>(ids.size()) < cap) {
					a_wheel = wheel;
					a_slot = s;
					a_why = "Auto slot already holding " + std::string(WeaponTypeName(a_key)) + " (" + std::to_string(ids.size()) + "/" + std::to_string(cap) + ")";
					return ByTypeRoute::Found;
				}
				++sameTypeFull;
			}
			if (autoSlots == 0) {
				a_why = "no slot is designated for " + std::string(SlotTypeName(a_type)) + " and no slot is Auto";
				return ByTypeRoute::NoSlot;
			}
			if (firstEmpty >= 0) {
				a_wheel = wheel;
				a_slot = firstEmpty;
				a_why = sameTypeFull > 0 ? ("first empty Auto slot (" + std::to_string(sameTypeFull) + " slot(s) of this type full)") : std::string("first empty Auto slot (no Auto slot holds this type yet)");
				return ByTypeRoute::Found;
			}
			a_why = sameTypeFull > 0 ? "no room: every Auto slot of this type is full and no Auto slot is empty" : "no room: no Auto slot holds this type and no Auto slot is empty";
			return ByTypeRoute::Full;
		}

		void Note(const std::string& a_what, bool a_added)
		{
			std::scoped_lock l(g_lock);
			if (a_added) { ++g_added; } else { ++g_skipped; }
			g_lastAction = a_what;
		}

		// Reads the player's favourites on the game thread and moves the new ones to their slots.
		void Scan()
		{
			g_scanQueued.store(false);
			if (!Config::Control::Wheel::FavoritesSystem) { return; }   // the master toggle gates the scan itself, whoever queued it
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return; }

			std::unordered_set<Key, KeyHash> now;
			std::unordered_set<RE::FormID> held;   // 1.6.1: inventory forms the player still carries
			auto inv = player->GetInventory();
			for (auto& [obj, data] : inv) {
				if (obj && data.first > 0) { held.insert(obj->GetFormID()); }
				if (!obj || !data.second || !data.second->IsFavorited()) { continue; }
				now.insert(Key{ obj->GetFormID(), UniqueIDOf(data.second.get()) });
			}
			if (auto* fav = RE::MagicFavorites::GetSingleton()) {
				for (RE::TESForm* f : fav->spells) {
					if (f) { now.insert(Key{ f->GetFormID(), 0 }); }
				}
			}

			std::unordered_set<RE::FormID> nowForms;
			for (const Key& k : now) { nowForms.insert(k.formID); }

			std::vector<Key> fresh;
			std::vector<RE::FormID> unfavourited;
			bool seeding = false;
			{
				std::scoped_lock l(g_lock);
				++g_scans;
				if (g_seeded) {
					for (const Key& k : now) { if (!g_known.contains(k)) { fresh.push_back(k); } }
					// 1.6.1 UN-FAVOURITING TAKES IT OFF THE WHEEL. A favourite at the last scan that is not one now, with
					// no other copy of the same form still a favourite, is un-favourited - but only while the player
					// still has it: an item sold, dropped or used up also stops being a favourite, and what happens to
					// it on the wheel stays with the inventory prune and the Keep Missing settings. Spells and shouts
					// are not inventory items and are taken at their word.
					for (const Key& k : g_known) {
						if (nowForms.contains(k.formID)) { continue; }
						auto* f = RE::TESForm::LookupByID(k.formID);
						if (!f) { continue; }
						const bool magic = f->GetFormType() == RE::FormType::Spell || f->GetFormType() == RE::FormType::Shout;
						if (!magic && !held.contains(k.formID)) { continue; }
						if (std::find(unfavourited.begin(), unfavourited.end(), k.formID) == unfavourited.end()) { unfavourited.push_back(k.formID); }
					}
				} else {
					seeding = true;
				}
				g_known = std::move(now);
				g_seeded = true;
			}

			if (seeding) {
				// 1.6.1: wheels read from a save written before 1.6.1 do not say who put each item there. The first scan
				// after the load settles it on the two favourites wheels: an item whose form is a favourite now counts as
				// put there by the favourites system, anything else as placed by hand.
				std::vector<int> wheels;
				for (int t = 0; t < kWheelCount; ++t) { wheels.push_back(WheelOfTable(t)); }
				const int settled = Wheeler::SettleUnknownSources(wheels, nowForms);
				if (settled > 0) { logger::info("[FavoritesToSlots] settled who placed {} item(s) read from an older save ({} favourite(s) now)", settled, nowForms.size()); }
			}

			for (RE::FormID id : unfavourited) {
				auto* form = RE::TESForm::LookupByID(id);
				const char* name = form && form->GetName() ? form->GetName() : "";
				std::vector<std::pair<int, int>> where;
				const int n = Wheeler::RemoveFavouriteItems(id, &where);
				if (n > 0) {
					std::string at;
					for (const auto& [w, s] : where) { at += (at.empty() ? "" : ", ") + std::to_string(w + 1) + "-" + std::to_string(s + 1); }
					{
						std::scoped_lock l(g_lock);
						g_removed += n;
						g_lastAction = std::string("removed ") + name + " from wheel slot " + at;
					}
					logger::info("[FavoritesToSlots] {} ({:08X}) un-favourited -> taken off wheel slot {} ({} item(s))", name, id, at, n);
				} else {
					logger::info("[FavoritesToSlots] {} ({:08X}) un-favourited; not on a wheel by a favourite (absent, or placed by hand) - left alone", name, id);
				}
			}

			for (const Key& k : fresh) {
				auto* form = RE::TESForm::LookupByID(k.formID);
				if (!form) { continue; }
				const Category cat = Classify(form);
				const char* name = form->GetName() ? form->GetName() : "";
				int wheel = -1, slot = -1;
				if (Config::SlotAssignments::WeaponTypeSlots && IsInventorySide(cat)) {
					// 1.5.0 By Type: the inventory wheel takes weapons, potions and poisons, one type per slot.
					const SlotType type = SlotTypeOf(form);
					if (type == SlotType::Auto) {
						Note(std::string("by type: no slot takes this kind of item, not added: ") + name, false);
						logger::info("[FavoritesToSlots] by type: {} ({:08X}, {}) is not a weapon, potion or poison - not added to the wheel", name, k.formID, CategoryName(cat));
						continue;
					}
					std::string why;
					const ByTypeRoute route = FindByTypeSlot(type, WeaponTypeKey(form), wheel, slot, why);
					if (route != ByTypeRoute::Found) {
						Note(std::string("by type: ") + why + ": " + name, false);
						logger::info("[FavoritesToSlots] by type: {} ({:08X}, type {}) - {}; not added", name, k.formID, SlotTypeName(type), why);
						if (route == ByTypeRoute::Full) { Utils::NotificationMessage(Texts::GetText(Texts::TextType::FavoriteSlotFull)); }
						continue;
					}
					logger::debug("[FavoritesToSlots] by type: {} ({:08X}, type {}) -> wheel {} slot {}: {}", name, k.formID, SlotTypeName(type), wheel + 1, slot + 1, why);
				} else if (!FindSlot(cat, wheel, slot)) {
					Note(std::string("no slot takes ") + CategoryName(cat) + ": " + name, false);
					logger::info("[FavoritesToSlots] {} ({:08X}) favourited; no slot on any wheel takes {} and none is Any - left alone", name, k.formID, CategoryName(cat));
					continue;
				}
				auto item = WheelItemFactory::MakeWheelItemFromFormID(k.formID, k.uniqueID);
				if (!item) {
					Note(std::string("not a wheel item: ") + name, false);
					logger::info("[FavoritesToSlots] {} ({:08X}) favourited; the wheel has no item type for it", name, k.formID);
					continue;
				}
				item->SetSource(WheelItem::Source::Favourite);   // 1.6.1: un-favouriting it takes it off again
				const auto result = Wheeler::AddItemToSlotOnWheel(wheel, slot, item);
				switch (result) {
				case Wheeler::AddToSlotResult::Added:
					{ std::scoped_lock l(g_lock); g_lastAddedFormID = k.formID; }
					Note(std::string("added ") + name + " to wheel " + std::to_string(wheel + 1) + " slot " + std::to_string(slot + 1), true);
					logger::info("[FavoritesToSlots] {} ({:08X}, {}) -> wheel {} slot {}", name, k.formID, CategoryName(cat), wheel + 1, slot + 1);
					Utils::NotificationMessage(std::string(name) + " " + Texts::GetText(Texts::TextType::FavoriteAddedToSlot) + " " + std::to_string(wheel + 1) + "-" + std::to_string(slot + 1));
					break;
				case Wheeler::AddToSlotResult::AlreadyOnWheel:
					Note(std::string("already on wheel ") + std::to_string(wheel + 1) + ": " + name, false);
					logger::info("[FavoritesToSlots] {} is already on wheel {}", name, wheel + 1);
					break;
				case Wheeler::AddToSlotResult::SlotFull:
					Note(std::string("slot full for ") + name, false);
					logger::info("[FavoritesToSlots] wheel {} slot {} is full; {} not added", wheel + 1, slot + 1, name);
					Utils::NotificationMessage(Texts::GetText(Texts::TextType::FavoriteSlotFull));
					break;
				case Wheeler::AddToSlotResult::NoSuchSlot:
					Note(std::string("wheel ") + std::to_string(wheel + 1) + " has no slot " + std::to_string(slot + 1), false);
					logger::info("[FavoritesToSlots] wheel {} has no slot {}; {} not added", wheel + 1, slot + 1, name);
					break;
				default:
					Note(std::string("no wheel ") + std::to_string(wheel + 1) + " for " + name, false);
					logger::info("[FavoritesToSlots] no wheel {}; {} not added", wheel + 1, name);
					break;
				}
			}
		}

		void Queue()
		{
			if (g_scanQueued.exchange(true)) { return; }
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks) { g_scanQueued.store(false); return; }
			tasks->AddTask([]() { Scan(); });
		}
	}

	const char* CategoryName(Category a_c)
	{
		switch (a_c) {
		case Category::Any: return "Any";
		case Category::Weapons: return "Weapons";
		case Category::Apparel: return "Apparel";
		case Category::WornArmor: return "Worn Armor";
		case Category::Potions: return "Potions";
		case Category::Scrolls: return "Scrolls";
		case Category::Food: return "Food";
		case Category::Ingredients: return "Ingredients";
		case Category::Spells: return "Spells";
		case Category::Powers: return "Powers";
		case Category::Shouts: return "Shouts";
		case Category::Ammo: return "Ammo";
		case Category::Misc: return "Misc";
		case Category::MeleeWeapons: return "Melee Weapons";
		case Category::RangedWeapons: return "Ranged Weapons";
		case Category::Alteration: return "Alteration";
		case Category::Illusion: return "Illusion";
		case Category::Destruction: return "Destruction";
		case Category::Conjuration: return "Conjuration";
		case Category::Restoration: return "Restoration";
		case Category::ShoutsAndPowers: return "Shouts and Powers";
		default: return "Unassigned";
		}
	}

	Category EffectiveCategory(int a_table, int a_slot, int a_slotCount)
	{
		if (a_table < 0 || a_table >= kWheelCount || a_slot < 0 || a_slot >= kSlotCount) { return Category::Unassigned; }
		if (a_table == 0 && a_slot == 0) { return Category::Powers; }                                    // the locked bottom slot
		if (a_table == 0 && a_slotCount >= 2 && a_slot == a_slotCount / 2) { return Category::Shouts; }   // the locked top slot
		return static_cast<Category>(Config::SlotAssignments::Slot[a_table][a_slot]);
	}

	bool Takes(Category a_slot, Category a_item)
	{
		if (a_slot == Category::Unassigned || a_item == Category::Unassigned) { return false; }
		if (a_slot == Category::Any || a_slot == a_item) { return true; }
		const bool school = a_item == Category::Alteration || a_item == Category::Illusion || a_item == Category::Destruction ||
							a_item == Category::Conjuration || a_item == Category::Restoration;
		if (a_slot == Category::Spells && school) { return true; }
		if (a_slot == Category::ShoutsAndPowers && (a_item == Category::Shouts || a_item == Category::Powers)) { return true; }
		if (a_slot == Category::Weapons && (a_item == Category::MeleeWeapons || a_item == Category::RangedWeapons)) { return true; }
		return false;
	}

	Category Classify(RE::TESForm* a_form)
	{
		if (!a_form) { return Category::Unassigned; }
		switch (a_form->GetFormType()) {
		case RE::FormType::Weapon: {
			// Bows, crossbows and staffs are ranged; everything else swings (the owner: "ranged weapons should get their
			// own slot separate from melee").
			auto* weap = a_form->As<RE::TESObjectWEAP>();
			if (!weap) { return Category::MeleeWeapons; }
			const auto t = weap->GetWeaponType();
			return (t == RE::WEAPON_TYPE::kBow || t == RE::WEAPON_TYPE::kCrossbow || t == RE::WEAPON_TYPE::kStaff) ? Category::RangedWeapons : Category::MeleeWeapons;
		}
		case RE::FormType::Armor: {
			// The four main worn pieces are their own category; everything else that is "armour" to
			// the game - shields, rings, amulets, cloaks, a mod's lantern - is Apparel.
			auto* armo = a_form->As<RE::TESObjectARMO>();
			if (!armo) { return Category::Apparel; }
			using Slot = RE::BGSBipedObjectForm::BipedObjectSlot;
#if RUNTIME_LINE == 17
			const auto mask = static_cast<std::uint32_t>(armo->GetSlotMask().underlying());   // 7.2: an EnumSet
#else
			const auto mask = static_cast<std::uint32_t>(armo->GetSlotMask());
#endif
			// Head is biped 30 OR 31: vanilla helmets occupy 31 (hair) + 42 (circlet), not 30 (the owner's first
			// test: Iron Helmet went to Apparel). A circlet alone (42) stays apparel, like jewellery.
			const auto main = static_cast<std::uint32_t>(Slot::kHead) | static_cast<std::uint32_t>(Slot::kHair) | static_cast<std::uint32_t>(Slot::kBody) |
							  static_cast<std::uint32_t>(Slot::kHands) | static_cast<std::uint32_t>(Slot::kFeet);
			return (mask & main) ? Category::WornArmor : Category::Apparel;
		}
		case RE::FormType::Spell: {
			auto* spell = a_form->As<RE::SpellItem>();
			if (!spell) { return Category::Spells; }
			const auto t = spell->GetSpellType();
			if (t == RE::MagicSystem::SpellType::kPower || t == RE::MagicSystem::SpellType::kLesserPower || t == RE::MagicSystem::SpellType::kVoicePower) { return Category::Powers; }
			// The school is the costliest effect's skill - what the magic menu files the spell under.
			RE::ActorValue skill = RE::ActorValue::kNone;
			if (auto* eff = spell->GetCostliestEffectItem(); eff && eff->baseEffect) { skill = eff->baseEffect->GetMagickSkill(); }
			switch (skill) {
			case RE::ActorValue::kAlteration: return Category::Alteration;
			case RE::ActorValue::kIllusion: return Category::Illusion;
			case RE::ActorValue::kDestruction: return Category::Destruction;
			case RE::ActorValue::kConjuration: return Category::Conjuration;
			case RE::ActorValue::kRestoration: return Category::Restoration;
			default: return Category::Spells;
			}
		}
		case RE::FormType::Shout: return Category::Shouts;
		case RE::FormType::AlchemyItem: {
			auto* alch = a_form->As<RE::AlchemyItem>();
			if (alch && alch->IsFood()) { return Category::Food; }
			return Category::Potions;   // potions and poisons share the Potions tab
		}
		case RE::FormType::Ingredient: return Category::Ingredients;
		case RE::FormType::Scroll: return Category::Scrolls;
		case RE::FormType::Ammo: return Category::Ammo;
		case RE::FormType::Light: return Category::Misc;   // torches
		case RE::FormType::Misc: return a_form->GetFormID() == 0x0000000F ? Category::Unassigned : Category::Misc;   // misc items the wheel can hold; never gold
		default: return Category::Unassigned;
		}
	}

	bool IsInventorySide(Category a_c)
	{
		switch (a_c) {
		case Category::Spells:
		case Category::Powers:
		case Category::Shouts:
		case Category::Alteration:
		case Category::Illusion:
		case Category::Destruction:
		case Category::Conjuration:
		case Category::Restoration:
		case Category::ShoutsAndPowers:
		case Category::Unassigned:
			return false;
		default:
			return true;
		}
	}

	std::uint32_t WeaponTypeKey(RE::TESForm* a_form)
	{
		if (!a_form || a_form->GetFormType() != RE::FormType::Weapon) { return 0; }
		auto* weap = a_form->As<RE::TESObjectWEAP>();
		if (!weap) { return 0; }
		const auto t = weap->GetWeaponType();
		if (t == RE::WEAPON_TYPE::kTwoHandAxe) {
			// Battleaxes and warhammers share the game's two-handed axe type; the vanilla keyword WeapTypeWarhammer
			// (Skyrim.esm 0x0006D930, its editor ID read from the master on 2026-10-20) tells them apart. Looked up at
			// the point of use until it resolves (rule 17); its editor ID is logged once when it does (rule 30).
			static RE::BGSKeyword* warhammer = nullptr;
			static bool loggedMiss = false;
			if (!warhammer) {
				warhammer = RE::TESForm::LookupByID<RE::BGSKeyword>(0x0006D930);
				if (warhammer) {
					const char* edid = warhammer->GetFormEditorID();
					logger::info("[FavoritesToSlots] warhammer keyword resolved: {:08X} '{}'", warhammer->GetFormID(), edid ? edid : "");
				} else if (!loggedMiss) {
					loggedMiss = true;
					logger::warn("[FavoritesToSlots] keyword WeapTypeWarhammer (0x0006D930) not found - warhammers share the battleaxe slot until it resolves");
				}
			}
			if (warhammer && weap->HasKeyword(warhammer)) { return kWeaponKeyWarhammer; }
		}
		return 0x100u + static_cast<std::uint32_t>(t);
	}

	const char* WeaponTypeName(std::uint32_t a_key)
	{
		if (a_key == kWeaponKeyWarhammer) { return "Warhammer"; }
		if (a_key < 0x100u) { return "not a weapon"; }
		switch (static_cast<RE::WEAPON_TYPE>(a_key - 0x100u)) {
		case RE::WEAPON_TYPE::kHandToHandMelee: return "Hand to Hand";
		case RE::WEAPON_TYPE::kOneHandSword: return "Sword";
		case RE::WEAPON_TYPE::kOneHandDagger: return "Dagger";
		case RE::WEAPON_TYPE::kOneHandAxe: return "War Axe";
		case RE::WEAPON_TYPE::kOneHandMace: return "Mace";
		case RE::WEAPON_TYPE::kTwoHandSword: return "Greatsword";
		case RE::WEAPON_TYPE::kTwoHandAxe: return "Battleaxe";
		case RE::WEAPON_TYPE::kBow: return "Bow";
		case RE::WEAPON_TYPE::kStaff: return "Staff";
		case RE::WEAPON_TYPE::kCrossbow: return "Crossbow";
		default: return "Other weapon";
		}
	}

	SlotType SlotTypeOf(RE::TESForm* a_form)
	{
		if (!a_form) { return SlotType::Auto; }
		if (a_form->GetFormType() == RE::FormType::AlchemyItem) {
			// Food stays food, as Classify has it.
			auto* alch = a_form->As<RE::AlchemyItem>();
			if (!alch || alch->IsFood()) { return SlotType::Auto; }
			return alch->IsPoison() ? SlotType::Poisons : SlotType::Potions;
		}
		const std::uint32_t key = WeaponTypeKey(a_form);
		if (key == kWeaponKeyWarhammer) { return SlotType::Warhammer; }
		if (key < 0x100u) { return SlotType::Auto; }
		switch (static_cast<RE::WEAPON_TYPE>(key - 0x100u)) {
		case RE::WEAPON_TYPE::kHandToHandMelee: return SlotType::HandToHand;
		case RE::WEAPON_TYPE::kOneHandSword: return SlotType::Sword;
		case RE::WEAPON_TYPE::kOneHandDagger: return SlotType::Dagger;
		case RE::WEAPON_TYPE::kOneHandAxe: return SlotType::WarAxe;
		case RE::WEAPON_TYPE::kOneHandMace: return SlotType::Mace;
		case RE::WEAPON_TYPE::kTwoHandSword: return SlotType::Greatsword;
		case RE::WEAPON_TYPE::kTwoHandAxe: return SlotType::Battleaxe;
		case RE::WEAPON_TYPE::kBow: return SlotType::Bow;
		case RE::WEAPON_TYPE::kStaff: return SlotType::Staff;
		case RE::WEAPON_TYPE::kCrossbow: return SlotType::Crossbow;
		default: return SlotType::Auto;
		}
	}

	const char* SlotTypeName(SlotType a_type)
	{
		switch (a_type) {
		case SlotType::Sword: return "Sword";
		case SlotType::Dagger: return "Dagger";
		case SlotType::WarAxe: return "War Axe";
		case SlotType::Mace: return "Mace";
		case SlotType::Greatsword: return "Greatsword";
		case SlotType::Battleaxe: return "Battleaxe";
		case SlotType::Warhammer: return "Warhammer";
		case SlotType::Bow: return "Bow";
		case SlotType::Crossbow: return "Crossbow";
		case SlotType::Staff: return "Staff";
		case SlotType::HandToHand: return "Hand to Hand";
		case SlotType::Potions: return "Potions";
		case SlotType::Poisons: return "Poisons";
		default: return "Auto";
		}
	}

	void Tick()
	{
		if (!Config::Control::Wheel::FavoritesSystem) {
			g_menuWasOpen = false;
			return;
		}
		const bool open = ItemMenuOpen();
		// Every 20 frames while an item menu is open, and once more when it has just closed, so a
		// favourite set on the last frame is not missed.
		if (open) {
			if (++g_framesSinceScan >= 20) { g_framesSinceScan = 0; Queue(); }
		} else if (g_menuWasOpen) {
			g_framesSinceScan = 0;
			Queue();
		}
		g_menuWasOpen = open;
	}

	void ScanNow() { Queue(); }

	void OnGameLoaded()
	{
		// 1.6.1: each loaded save starts a fresh snapshot - without this the favourites of the save loaded before were
		// compared with this one's, and what the two saves did not share read as favourited or un-favourited.
		{
			std::scoped_lock l(g_lock);
			g_known.clear();
			g_seeded = false;
		}
		g_framesSinceScan = 0;
		if (Config::Control::Wheel::FavoritesSystem) { Queue(); }   // the snapshot is taken at the load, not at the first menu
	}

	void UnmarkForTest(RE::FormID a_formID)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) { return; }
		tasks->AddTask([a_formID]() {
			RE::FormID id = a_formID;
			if (id == 0) { std::scoped_lock l(g_lock); id = g_lastAddedFormID; }
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* form = id ? RE::TESForm::LookupByID(id) : nullptr;
			if (!player || !form) { logger::info("[FavoritesToSlots] test unmark: no form {:08X}", id); return; }
			int done = 0;
			if (form->GetFormType() == RE::FormType::Spell || form->GetFormType() == RE::FormType::Shout) {
				if (auto* fav = RE::MagicFavorites::GetSingleton()) { fav->RemoveFavorite(form); ++done; }
			} else if (auto* changes = player->GetInventoryChanges(); changes && changes->entryList) {
				for (auto* entry : *changes->entryList) {
					if (!entry || entry->object != form || !entry->extraLists) { continue; }
					for (auto* xl : *entry->extraLists) {
						if (xl && xl->HasType(RE::ExtraDataType::kHotkey)) { changes->RemoveFavorite(entry, xl); ++done; }
					}
				}
			}
			logger::info("[FavoritesToSlots] test unmark {} ({:08X}): {} favourite(s) removed through the game", form->GetName() ? form->GetName() : "", id, done);
			Scan();
		});
	}

	// Driving-tool entry, test only: forget the snapshot so EVERY current favourite counts as new on the next
	// scan - fills fresh wheels from a save that already has favourites, for a proof or a capture.
	void AdoptAllForTest()
	{
		{
			std::scoped_lock l(g_lock);
			g_known.clear();
			g_seeded = true;
		}
		Queue();
	}

	void MarkForTest(int a_count)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) { return; }
		tasks->AddTask([a_count]() {
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) { return; }
			int items = 0, magic = 0;
			if (auto* changes = player->GetInventoryChanges(); changes && changes->entryList) {
				for (auto* entry : *changes->entryList) {
					if (items >= a_count) { break; }
					if (!entry || !entry->object || entry->IsFavorited() || !entry->object->GetPlayable()) { continue; }
					RE::ExtraDataList* xl = (entry->extraLists && !entry->extraLists->empty()) ? entry->extraLists->front() : nullptr;
					changes->SetFavorite(entry, xl);
					++items;
					logger::info("[FavoritesToSlots] test mark item {} ({:08X})", entry->GetDisplayName() ? entry->GetDisplayName() : "", entry->object->GetFormID());
				}
			}
			auto* fav = RE::MagicFavorites::GetSingleton();
			auto mark = [&](RE::TESForm* f) {
				if (!f || !fav || magic >= a_count) { return; }
				for (RE::TESForm* s : fav->spells) { if (s == f) { return; } }
				fav->SetFavorite(f);
				++magic;
				logger::info("[FavoritesToSlots] test mark magic {} ({:08X})", f->GetName() ? f->GetName() : "", f->GetFormID());
			};
			for (RE::SpellItem* s : player->GetActorRuntimeData().addedSpells) { mark(s); }
			if (auto* base = player->GetActorBase(); base && base->actorEffects) {
				for (std::uint32_t i = 0; i < base->actorEffects->numSpells; ++i) { mark(base->actorEffects->spells[i]); }
				for (std::uint32_t i = 0; i < base->actorEffects->numShouts; ++i) { mark(base->actorEffects->shouts[i]); }
			}
			if (auto* race = player->GetRace(); race && race->actorEffects) {
				for (std::uint32_t i = 0; i < race->actorEffects->numSpells; ++i) { mark(race->actorEffects->spells[i]); }
			}
			logger::info("[FavoritesToSlots] test mark: {} items, {} spells/shouts favourited", items, magic);
			Scan();
		});
	}

	std::string StatusJson()
	{
		std::scoped_lock l(g_lock);
		std::string esc; for (char c : g_lastAction) { if (c == '"' || c == '\\') { esc += '\\'; } esc += c; }
		std::string cats;
		for (int w = 0; w < kWheelCount; ++w) {
			cats += (w ? ",[" : "[");
			for (int i = 0; i < kSlotCount; ++i) { cats += (i ? "," : ""); cats += std::to_string(Config::SlotAssignments::Slot[w][i]); }
			cats += "]";
		}
		std::string types;
		for (int i = 0; i < kSlotCount; ++i) { types += (i ? "," : ""); types += std::to_string(Config::SlotAssignments::Type[i]); }
		const int inv = Wheeler::FindWheelIndexByRole("inventory"), mag = Wheeler::FindWheelIndexByRole("magic");
		return "{\"enabled\":" + std::string(Config::Control::Wheel::FavoritesSystem ? "true" : "false") + ",\"skyUICategories\":" + std::string(Config::SlotAssignments::SkyUICategories ? "true" : "false") + ",\"weaponTypeSlots\":" + std::string(Config::SlotAssignments::WeaponTypeSlots ? "true" : "false") + ",\"slotTypes\":[" + types + "],\"inventoryWheel\":" + std::to_string(inv) + ",\"magicWheel\":" + std::to_string(mag) + ",\"seeded\":" + (g_seeded ? "true" : "false") +
			",\"known\":" + std::to_string(g_known.size()) + ",\"scans\":" + std::to_string(g_scans) + ",\"added\":" + std::to_string(g_added) +
			",\"skipped\":" + std::to_string(g_skipped) + ",\"removed\":" + std::to_string(g_removed) + ",\"lastAdded\":" + std::to_string(g_lastAddedFormID) + ",\"categories\":[" + cats + "],\"last\":\"" + esc + "\"}";
	}
}
