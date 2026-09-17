#pragma once
#include <array>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include "nlohmann/json.hpp"
#include "WheelEntry.h"

class Wheel
{
public:
    Wheel();
    ~Wheel();

	struct MouseHoverDebugInfo
	{
		bool valid = false;
		bool useNewModel = false;
		float rNorm = 0.0f;
		float speed = 0.0f;
		float speedNorm = 0.0f;
		float rawTheta = 0.0f;
		float stableTheta = 0.0f;
		float deltaTheta = 0.0f;
		float dwellTime = 0.0f;
		int currentIdx = -1;
		int bestIdx = -1;
		int secondIdx = -1;
		int intentIdx = -1;
		bool intentActive = false;
		float bestScore = 0.0f;
		float secondScore = 0.0f;
		std::array<char, 64> reason{};
	};

	void Draw(ImVec2 a_wheelCenter, ImVec2 a_cursorPos, float a_cursorAngle, bool a_cursorCentered,
		RE::TESObjectREFR::InventoryItemMap& a_imap, DrawArgs a_drawArgs,
		float a_hoveredEntryTimeSeconds = 0.0f, float a_hoverActivateDelaySeconds = 0.0f,
		float a_deltaTimeSeconds = 0.0f);
		

    void Clear();
    bool IsEmpty();
    
    void PushEntry(std::unique_ptr<WheelEntry> a_entry);
	void PushEmptyEntry();

    void PrevItemInHoveredEntry();
    void NextItemInHoveredEntry();
	
	void ResetAnimation();

    /// <summary>
	/// Activate the entry using a primary input(mouse left click / controller right trigger), which either activates
	/// the currently active item in the entry, or, under edit mode, adds a new item to the entry(if applicable).
    /// </summary>
    void ActivateHoveredEntryPrimary(bool a_editMode);

	/// <summary>
	/// Activate the entry using a secondary input(mouse right click / controller left trigger), which either deletes
	/// an item in the entry, or the whole entry when it's empty.
	/// </summary>
	/// <param name="a_editMode">Whether we're in edit mode, which prompts us to deletion.</param>
	void ActivateHoveredEntrySecondary(bool a_editMode);

	void ActivateHoveredEntrySpecial(bool a_editMode);

	// Removes depleted consumables (alchemy items with 0 count) from their slots.
	// Does not remove entries; it just clears the item(s) from the entry so the slot becomes empty.
	void ClearDepletedConsumables();
	void SetHoveredEntryIndex(int a_index);
	int GetHoveredEntryIndex() const { return _hoveredEntryIdx; }
	std::shared_ptr<WheelItem> GetHoveredSelectedItem();
	const MouseHoverDebugInfo& GetMouseHoverDebugInfo() const { return _mouseHoverState.debug; }

	void MoveHoveredEntryForward();
	void MoveHoveredEntryBack();
	// 1.2.8 (Barbadoza on the Nexus page, the owner's shape of it): pick the hovered slot up with
	// everything in it, move the cursor, press again to drop it there: the two slots SWAP places,
	// full or empty, so nothing else on the wheel moves. Pressing on the held slot itself puts it
	// back. Returns what happened, for the log and the driving tool.
	const char* PickUpOrDropHoveredEntry();
	int GetHeldEntryIndex() const { return _heldEntryIdx; }
	void ClearHeldEntry();
	void SetDriveHover(int a_index) { _driveHoverIdx = a_index; }

	void SerializeIntoJsonObj(nlohmann::json& a_json);
	static std::unique_ptr<Wheel> SerializeFromJsonObj(const nlohmann::json& a_json, SKSE::SerializationInterface* a_intfc);

	void SetClientTag(std::string_view tag) { _clientTag.assign(tag); }
	const std::string& GetClientTag() const { return _clientTag; }
	bool HasClientTag() const { return !_clientTag.empty(); }
	
	int GetNumEntries();
	
	// Inventory prune support
	WheelEntry* GetEntry(int index);
	void RemoveEntryByIndex(int index);
	
	// Clears all items from an entry, making it empty while preserving the slot index.
	// This is the preferred method for inventory pruning to maintain index stability.
	// Returns true if entry was cleared, false if index out of range or already empty.
	bool ClearEntryByIndex(int index);

private:
	struct MouseHoverState
	{
		bool initialized = false;
		int lastEntryCount = 0;
		ImVec2 lastCursorPos{ 0.0f, 0.0f };
		ImVec2 filteredVelocity{ 0.0f, 0.0f };
		ImVec2 stableDir{ 0.0f, 1.0f };
		float stableTheta = 0.0f;
		float rawTheta = 0.0f;
		float dwellTime = 0.0f;
		int lastHoverIdx = -1;
		int lastCommittedIdx = -1;
		bool hadValidHoverThisOpen = false;
		bool loggedStabilizationConfigThisOpen = false;
		bool intentActive = false;
		int intentTargetIdx = -1;
		int intentCandidateIdx = -1;
		float intentHoldSec = 0.0f;
		int pendingSwitchIdx = -1;
		float pendingSwitchSec = 0.0f;
		int lastLoggedIntentIdx = -1;
		bool lastLoggedIntentActive = false;
		MouseHoverDebugInfo debug{};
	};

	std::string _clientTag;
    std::vector<std::unique_ptr<WheelEntry>> _entries = {};
	std::shared_mutex _lock;
	
	// currently active item, will be highlighted. Gets reset every time wheel reopens.
	int _hoveredEntryIdx = -1;
	int _heldEntryIdx = -1;   // the slot picked up in edit mode, -1 when none
	// The driving tool's hover: the cursor recomputes _hoveredEntryIdx every frame and returns
	// to -1 the moment nothing deflects it, so a driven hover has to be held FOR it. -1 = none.
	int _driveHoverIdx = -1;
	MouseHoverState _mouseHoverState{};
	
};
