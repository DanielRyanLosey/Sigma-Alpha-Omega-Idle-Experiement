#pragma once
#include "SAO_Game_Conductor_Cache_Node.h"
#include "SAO_Toth.h"
#include "SAO_Gear_Inventory_Conductor.h"
#include <cstdint>
#include <string>

// Last cache stage. Prime and Toth only FORWARD here; Program only receives this.
namespace SAO_Prime_Cache_Node
{
    constexpr UINT SAO_INPUT = WM_APP + 1;
    using GameNode = SAO_Game_Conductor_Cache_Node::OutputNode;

    // Numeric harvest projection: output-only, received by Program.
    struct HarvestNumbers
    {
        UINT health = 100;
        UINT healthUnits = 20000; // HP in exact 0.005 units; display health rounds up.
        UINT maxHealth = 100;
        UINT completedHits = 0;
        UINT removedBushels = 0;
        UINT grassItems = 0;
        UINT strawItems = 0;
        UINT lastDamage = 0;
        bool swinging = false;
    };

    struct OutputNode
    {
        const GameNode* game = nullptr;
        HBITMAP bitmap = nullptr;       // Original game DIB, reused, not duplicated.
        BYTE* pixels = nullptr;
        UINT frame = 0;
        HarvestNumbers harvest{};
        std::uint64_t overlayRevision = 0;
        bool ready = false;            // First visible Prime + Toth frame is ready.
        bool animation_ready = false;  // Prime frames + both static Toth sources ready.
        bool preload_finished = false;
        std::wstring error;
        std::wstring preload_error;
    };

    extern OutputNode Cache;

    bool Start(HWND program);
    bool Build(const GameNode& game, HWND program); // Prime -> shared node
    bool ReceiveToth(const SAO_Toth::OutputNode& toth, HWND program); // Toth -> shared
    bool ReceiveInventory(const SAO_Inventory_Conductor::OutputNode& inventory,
        const SAO_Gear_Conductor::OutputNode& gear,
        const SAO_Stone_Sickle_Conductor::OutputNode& sickle);
    void Preload();                                // Program's existing 2s signal
    bool Advance();                               // Original Prime frame + reused static Toth pixels
    bool Present(HDC destination);
    bool Click(std::uint32_t x, std::uint32_t y, HWND program);
    bool MouseMove(std::uint32_t x, std::uint32_t y);
    bool MouseUp(std::uint32_t x, std::uint32_t y);
    bool RightClick(std::uint32_t x, std::uint32_t y);
    bool InventoryDragging();
    void Stop();
}

