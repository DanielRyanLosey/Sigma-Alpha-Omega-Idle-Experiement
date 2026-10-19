#pragma once
#include "SAO_Inventory_Conductor.h"
#include "SAO_Gear_Conductor.h"
#include "SAO_Stone_Sickle_Conductor.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <windows.h>

namespace SAO_Gear_Inventory_Conductor {
    constexpr std::uint32_t RESOURCE_MAX = 10000;
    constexpr std::uint32_t EQUIPMENT_MAX = 1;
    enum class Polarity :int { Plus = 1, Minus = -1 };
    enum class Kind :std::uint8_t { Empty = 0, Grass = 1, Straw = 2, Equipment = 3 };
    struct Item {
        Kind kind = Kind::Empty;
        std::uint32_t amount = 0;
        std::uint32_t equipmentId = 0; // stable id for a future item registry
        std::uint32_t gearSlot = UINT32_MAX; // permitted target; UINT32_MAX = not equipped
    };
    struct ArtPixel { int x = 0, y = 0; std::array<BYTE, 4> bgra{}; };
    struct ItemArt {
        std::wstring path;
        int width = 0, height = 0;
        std::vector<ArtPixel> pixels;
    };
    struct Drag {
        bool active = false, fromGear = false;
        unsigned source = 0;
        int x = 0, y = 0;
        Item item{};
    };
    struct OutputNode {
        const SAO_Inventory_Conductor::OutputNode* inventory = nullptr;
        const SAO_Gear_Conductor::OutputNode* gear = nullptr;
        Polarity inventorySign = Polarity::Minus;
        Polarity gearSign = Polarity::Minus;
        std::array<Item, 16> items{};
        std::array<Item, 13> equipment{};
        Drag drag{};
        ItemArt grassArt, strawArt;
        const SAO_Stone_Sickle_Conductor::OutputNode* sickle = nullptr;
        Polarity toolSign = Polarity::Minus;
        std::array<ItemArt, 16> sickleInventoryArt{};
        ItemArt sickleToolArt;
        std::vector<std::wstring> unlocks;
        std::uint64_t revision = 0;
        bool ready = false, panelsVisible = false;
        std::wstring error;
    };
    extern OutputNode Cache;
    // Sources arrive already bitmapped and grouped. No backward Math/Conductor calls.
    bool Build(const SAO_Inventory_Conductor::OutputNode& inventory,
        const SAO_Gear_Conductor::OutputNode& gear,
        const SAO_Stone_Sickle_Conductor::OutputNode& sickle);
    bool ReceiveItemArt(bool straw, const std::wstring& path, UINT width, UINT height);
    bool SetProfile(const std::wstring& playerName);
    bool OpenBoth(); // +Inventory +Gear; Toth view is selected by its own Conductor
    bool CloseBoth();
    bool ReceiveHarvest(std::uint32_t cumulativeGrass, std::uint32_t cumulativeStraw);
    bool MouseDown(int x, int y);
    bool MouseMove(int x, int y);
    bool MouseUp(int x, int y);
    bool RightDelete(int x, int y);
    bool RegisterEquipment(std::uint32_t id, std::uint32_t gearTarget);
    bool SetUnlocked(const std::wstring& name, bool unlocked);
    bool IsUnlocked(const std::wstring& name);
    bool Load();
    bool Save();
    void Stop();
}
