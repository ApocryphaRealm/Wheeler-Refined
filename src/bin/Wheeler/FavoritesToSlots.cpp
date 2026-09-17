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
		int g_scans = 0, g_added = 0, g_skipped = 0;
		std::string g_lastAction;

		bool ItemMenuOpen()
		{
			auto* ui = RE::UI::GetSingleton();
			return ui && (ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME) || ui->IsMenuOpen(RE::MagicMenu::MENU_NAME) ||
						  ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) || ui->IsMenuOpen(RE::BarterMenu::MENU_NAME));
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
		bool FindSlot(Category a_item, int& a_wheel, int& a_slot)
		{
			int wheelOf[kWheelCount];
			for (int w = 0; w < kWheelCount; ++w) {
				const int byRole = Wheeler::FindWheelIndexByRole(kRole[w]);
				wheelOf[w] = byRole >= 0 ? byRole : w;
			}
			for (int pass = 0; pass < 2; ++pass) {
				for (int w = 0; w < kWheelCount; ++w) {
					const int count = Wheeler::GetSlotCountOfWheel(wheelOf[w]);
					if (count <= 0) { continue; }
					for (int s = 0; s < kSlotCount && s < count; ++s) {
						const auto cat = EffectiveCategory(w, s, count);
						if (pass == 0 ? (cat != Category::Any && Takes(cat, a_item)) : (cat == Category::Any)) { a_wheel = wheelOf[w]; a_slot = s; return true; }
					}
				}
			}
			return false;
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
			auto inv = player->GetInventory();
			for (auto& [obj, data] : inv) {
				if (!obj || !data.second || !data.second->IsFavorited()) { continue; }
				now.insert(Key{ obj->GetFormID(), UniqueIDOf(data.second.get()) });
			}
			if (auto* fav = RE::MagicFavorites::GetSingleton()) {
				for (RE::TESForm* f : fav->spells) {
					if (f) { now.insert(Key{ f->GetFormID(), 0 }); }
				}
			}

			std::vector<Key> fresh;
			{
				std::scoped_lock l(g_lock);
				++g_scans;
				if (g_seeded) {
					for (const Key& k : now) { if (!g_known.contains(k)) { fresh.push_back(k); } }
				}
				g_known = std::move(now);
				g_seeded = true;
			}

			for (const Key& k : fresh) {
				auto* form = RE::TESForm::LookupByID(k.formID);
				if (!form) { continue; }
				const Category cat = Classify(form);
				const char* name = form->GetName() ? form->GetName() : "";
				int wheel = -1, slot = -1;
				if (!FindSlot(cat, wheel, slot)) {
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
				const auto result = Wheeler::AddItemToSlotOnWheel(wheel, slot, item);
				switch (result) {
				case Wheeler::AddToSlotResult::Added:
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
			const auto mask = static_cast<std::uint32_t>(armo->GetSlotMask());
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
		const int inv = Wheeler::FindWheelIndexByRole("inventory"), mag = Wheeler::FindWheelIndexByRole("magic");
		return "{\"enabled\":" + std::string(Config::Control::Wheel::FavoritesSystem ? "true" : "false") + ",\"inventoryWheel\":" + std::to_string(inv) + ",\"magicWheel\":" + std::to_string(mag) + ",\"seeded\":" + (g_seeded ? "true" : "false") +
			",\"known\":" + std::to_string(g_known.size()) + ",\"scans\":" + std::to_string(g_scans) + ",\"added\":" + std::to_string(g_added) +
			",\"skipped\":" + std::to_string(g_skipped) + ",\"categories\":[" + cats + "],\"last\":\"" + esc + "\"}";
	}
}
