#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_Gear_Inventory_Conductor.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cwchar>
#include <climits>

namespace SAO_Gear_Inventory_Conductor {
    OutputNode Cache{};
    namespace {
        namespace fs = std::filesystem;
        std::wstring profile;
        std::uint32_t seenGrass = 0, seenStraw = 0;
        std::uint32_t heldGrass = 0, heldStraw = 0; // Overflow is retained numerically.
        bool dirty = false;
        bool profileLoaded = false;
        constexpr const wchar_t* STARTER = L"Starter_Stone_Sickle_Granted";
        void SelectTool() {
            const auto& tool = Cache.equipment[SAO_Stone_Sickle_Conductor::TOOL_SLOT];
            Cache.toolSign = tool.kind == Kind::Equipment &&
                tool.equipmentId == SAO_Stone_Sickle_Conductor::ITEM_ID ? Polarity::Plus : Polarity::Minus;
        }
        bool Starter() {
            if (!profileLoaded || !Cache.sickle || !Cache.sickle->ready)return false;
            if (std::find(Cache.unlocks.begin(), Cache.unlocks.end(), STARTER) != Cache.unlocks.end())return true;
            // Adoption also prevents a second copy if this item already exists in a save.
            auto isSickle = [](const Item& item) {return item.kind == Kind::Equipment &&
                item.equipmentId == SAO_Stone_Sickle_Conductor::ITEM_ID; };
            bool present = std::any_of(Cache.items.begin(), Cache.items.end(), isSickle) ||
                std::any_of(Cache.equipment.begin(), Cache.equipment.end(), isSickle);
            if (!present) {
                auto free = std::find_if(Cache.items.begin(), Cache.items.end(),
                    [](const Item& item) {return item.kind == Kind::Empty; });
                if (free == Cache.items.end())return true; // Defer until a slot is freed.
                *free = { Kind::Equipment,1,SAO_Stone_Sickle_Conductor::ITEM_ID,
                    SAO_Stone_Sickle_Conductor::TOOL_SLOT };
            }
            Cache.unlocks.push_back(STARTER); dirty = true; ++Cache.revision; return true;
        }
        constexpr char HEADER[8] = { 'S','A','O','B','A','G','0','1' };
        constexpr std::uint32_t VERSION = 1;
        struct Mask { UINT width = 0, height = 0; std::vector<std::uint32_t> pixels; };
        std::array<Mask, 16> inventoryMasks;
        std::array<Mask, 13> gearMasks;
        bool ReadMask(const SAO_Static_PNG_Cache::BitmapNode* node, Mask& mask) {
            if (!node)return false;
            std::vector<BYTE> bytes;
            if (!SAO_Static_PNG_Cache::Load(*node, bytes))return false;
            mask = {}; mask.width = node->width; mask.height = node->height;
            for (std::size_t i = 0; i < bytes.size() / 4; ++i)
                if (bytes[i * 4 + 3])mask.pixels.push_back(static_cast<std::uint32_t>(i));
            return !mask.pixels.empty();
        }
        bool FitSickle(const SAO_Stone_Sickle_Conductor::BitmapGroup& source,
            const Mask& location, ItemArt& output) {
            if (location.pixels.empty() || source.pixels.empty())return false;
            unsigned left = location.width, top = location.height, right = 0, bottom = 0;
            for (auto offset : location.pixels) {
                const unsigned x = offset % location.width, y = offset / location.width;
                left = (std::min)(left, x); right = (std::max)(right, x);
                top = (std::min)(top, y); bottom = (std::max)(bottom, y);
            }
            const int boxW = int(right - left + 1), boxH = int(bottom - top + 1);
            const int sw = source.right - source.left + 1, sh = source.bottom - source.top + 1;
            if (sw <= 0 || sh <= 0)return false;
            int w = sw, h = sh;
            // Keep native size if it fits; otherwise uniformly fit inside the location.
            if (w > boxW || h > boxH) {
                if (std::int64_t(boxW) * sh <= std::int64_t(boxH) * sw) {
                    w = boxW; h = (std::max)(1, int(std::int64_t(sh) * w / sw));
                }
                else {
                    h = boxH; w = (std::max)(1, int(std::int64_t(sw) * h / sh));
                }
            }
            output = {}; output.width = boxW; output.height = boxH;
            const int dx = (boxW - w) / 2, dy = (boxH - h) / 2;
            // Sparse source is sorted in original row-major order. No PNG re-decode.
            for (int y = 0; y < h; ++y)for (int x = 0; x < w; ++x) {
                const UINT sx = UINT(source.left + std::int64_t(x) * sw / w);
                const UINT sy = UINT(source.top + std::int64_t(y) * sh / h);
                const auto key = std::uint64_t(sy) * source.width + sx;
                const auto it = std::lower_bound(source.pixels.begin(), source.pixels.end(), key,
                    [&](const SAO_Stone_Sickle_Conductor::Pixel& p, std::uint64_t k) {
                        return std::uint64_t(p.y) * source.width + p.x < k; });
                if (it != source.pixels.end() && it->x == sx && it->y == sy)
                    output.pixels.push_back({ x + dx,y + dy,it->bgra });
            }
            return true;
        }
        const wchar_t* GearNames[13] = { L"Boots",L"Bracers",L"Hands",L"Head",
            L"Legs",L"Necklace",L"Ring",L"Shoulders",L"Tool",L"Torso",
            L"Trinket_1",L"Trinket_2",L"Weapon" };
        fs::path Saves() {
            wchar_t buf[MAX_PATH]{};
            const DWORD size = GetModuleFileNameW(nullptr, buf, MAX_PATH);
            if (!size || size >= MAX_PATH)return {};
            fs::path root = fs::path(std::wstring(buf, size)).parent_path();
            for (int i = 0; i < 8; ++i) {
                const fs::path a = root / L"SAO_I_E Assets";
                std::error_code ec;
                if (fs::is_directory(a, ec))return a / L"Player Saves";
                const fs::path parent = root.parent_path();
                if (parent == root || parent.empty())break;
                root = parent;
            }
            return {};
        }
        bool ValidProfile(const std::wstring& s) {
            if (s.empty() || s.size() > 64)return false;
            for (wchar_t c : s) {
                if (c < 32 || wcschr(L"\\/:*?\"<>|", c))return false;
            }
            return s != L"." && s != L"..";
        }
        fs::path SavePath() { return Saves() / (profile + L"_Inventory_Gear.sao"); }
        std::uint32_t Max(Kind k) { return k == Kind::Equipment ? EQUIPMENT_MAX : RESOURCE_MAX; }
        void Clear(Item& item) { item = Item{}; }
        bool Same(const Item& a, const Item& b) {
            return a.kind == b.kind &&
                (a.kind != Kind::Equipment || a.equipmentId == b.equipmentId);
        }
        // Fill existing stack first, then first available slot. Never discard overflow.
        void Add(Kind kind, std::uint32_t amount) {
            for (int pass = 0; pass < 2 && amount; ++pass) {
                for (auto& s : Cache.items) {
                    if (amount == 0)break;
                    if (pass == 0 && (s.kind != kind || s.amount >= Max(kind)))continue;
                    if (pass == 1 && s.kind != Kind::Empty)continue;
                    if (s.kind == Kind::Empty)s.kind = kind;
                    const auto free = Max(kind) - s.amount;
                    const auto used = (std::min)(free, amount);
                    s.amount += used; amount -= used;
                    if (used)dirty = true;
                }
            }
            if (kind == Kind::Grass && amount) { heldGrass += amount; dirty = true; }
            if (kind == Kind::Straw && amount) { heldStraw += amount; dirty = true; }
        }
        void FlushOverflow() {
            std::uint32_t g = heldGrass, s = heldStraw;
            heldGrass = heldStraw = 0;
            Add(Kind::Grass, g); Add(Kind::Straw, s);
        }
        // Hit only opaque pixels of the supplied location PNG; no radius.
        int Hit(int x, int y, bool gear) {
            const unsigned count = gear ? unsigned(gearMasks.size()) : unsigned(inventoryMasks.size());
            for (unsigned i = 0; i < count; ++i) {
                const auto& mask = gear ? gearMasks[i] : inventoryMasks[i];
                const int localX = x - (!gear && mask.width == 960 ? 960 : 0);
                if (localX < 0 || y < 0 || unsigned(localX) >= mask.width || unsigned(y) >= mask.height)continue;
                const auto index = std::uint32_t(y) * mask.width + unsigned(localX);
                if (std::binary_search(mask.pixels.begin(), mask.pixels.end(), index))return int(i);
            }
            return -1;
        }
        bool Put(Item moving, Item& dest, Item& origin, bool toGear, bool fromGear, int target) {
            if (toGear && (moving.kind != Kind::Equipment || moving.gearSlot != unsigned(target)))
                return false;
            if (!toGear && moving.kind == Kind::Equipment && moving.amount != 1)
                return false;
            if (dest.kind == Kind::Empty) { dest = moving; Clear(origin); return true; }
            if (!toGear && Same(dest, moving) && moving.kind != Kind::Equipment) {
                const auto amount = (std::min)(RESOURCE_MAX - dest.amount, moving.amount);
                if (!amount)return false;
                dest.amount += amount; origin.amount -= amount;
                if (!origin.amount)Clear(origin);
                return true;
            }
            if (toGear) {
                // Remove occupied equipment to the vacated inventory slot, if any.
                // Otherwise use the next free inventory slot; no item is destroyed.
                if (dest.kind != Kind::Equipment)return false;
                if (!fromGear) {
                    const Item old = dest;
                    dest = moving; origin = old; return true;
                }
                return false;
            }
            // Requested inventory spot occupied by a different item:
            // choose the next empty slot deterministically before considering a swap.
            for (auto& free : Cache.items) {
                if (free.kind == Kind::Empty) { free = moving; Clear(origin); return true; }
            }
            if (fromGear)return false; // A resource may not end up in a Gear location.
            std::swap(dest, origin);
            return true;
        }
        template<typename T> bool Write(std::ofstream& stream, const T& data) {
            stream.write(reinterpret_cast<const char*>(&data), sizeof(T)); return bool(stream);
        }
        template<typename T> bool Read(std::ifstream& stream, T& data) {
            return bool(stream.read(reinterpret_cast<char*>(&data), sizeof(T)));
        }
        bool ReadItem(std::ifstream& in, Item& item) {
            std::uint8_t kind = 0;
            if (!Read(in, kind) || !Read(in, item.amount) || !Read(in, item.equipmentId)
                || !Read(in, item.gearSlot) || kind > 3)return false;
            item.kind = Kind(kind);
            if ((kind == 0 && item.amount != 0) ||
                (kind != 0 && (item.amount<1 || item.amount>Max(item.kind))))return false;
            return true;
        }
        bool WriteItem(std::ofstream& out, const Item& item) {
            return Write(out, static_cast<std::uint8_t>(item.kind)) &&
                Write(out, item.amount) && Write(out, item.equipmentId) && Write(out, item.gearSlot);
        }
    }
    bool Build(const SAO_Inventory_Conductor::OutputNode& inventory,
        const SAO_Gear_Conductor::OutputNode& gear,
        const SAO_Stone_Sickle_Conductor::OutputNode& sickle) {
        if (!inventory.ready || !gear.ready) {
            Cache.error = !inventory.ready ? inventory.error : gear.error; return false;
        }
        for (unsigned i = 0; i < inventoryMasks.size(); ++i)
            if (!ReadMask(inventory.group.locations[i], inventoryMasks[i])) {
                Cache.error = L"Inventory location mask could not be loaded."; return false;
            }
        for (unsigned i = 0; i < gearMasks.size(); ++i)
            if (!ReadMask(gear.group.locations[i], gearMasks[i])) {
                Cache.error = L"Gear location mask could not be loaded."; return false;
            }
        if (!sickle.ready) { Cache.error = sickle.error; return false; }
        for (unsigned i = 0; i < inventoryMasks.size(); ++i)
            if (!FitSickle(sickle.item, inventoryMasks[i], Cache.sickleInventoryArt[i])) {
                Cache.error = L"Sickle inventory padding failed."; return false;
            }
        if (!FitSickle(sickle.item, gearMasks[SAO_Stone_Sickle_Conductor::TOOL_SLOT], Cache.sickleToolArt)) {
            Cache.error = L"Sickle Tool padding failed."; return false;
        }
        Cache.sickle = &sickle;
        Cache.inventory = &inventory; Cache.gear = &gear;
        Cache.ready = true; ++Cache.revision; return true;
    }
    bool ReceiveItemArt(bool straw, const std::wstring& path, UINT width, UINT height) {
        auto& art = straw ? Cache.strawArt : Cache.grassArt;
        if (art.path == path && !art.pixels.empty())return true;
        if (path.empty() || !width || !height || width > INT_MAX || height > INT_MAX)return false;
        std::ifstream input(fs::path(path), std::ios::binary);
        if (!input)return false;
        ItemArt next; next.path = path;
        int left = int(width), top = int(height), right = -1, bottom = -1;
        std::vector<BYTE> row(std::size_t(width) * 4);
        for (UINT y = 0; y < height; ++y) {
            if (!input.read(reinterpret_cast<char*>(row.data()), row.size()))return false;
            for (UINT x = 0; x < width; ++x) {
                const BYTE* p = row.data() + std::size_t(x) * 4;
                if (!p[3])continue;
                next.pixels.push_back({ int(x),int(y),{p[0],p[1],p[2],p[3]} });
                left = (std::min)(left, int(x)); top = (std::min)(top, int(y));
                right = (std::max)(right, int(x)); bottom = (std::max)(bottom, int(y));
            }
        }
        if (next.pixels.empty() || input.peek() != EOF)return false;
        next.width = right - left + 1; next.height = bottom - top + 1;
        for (auto& p : next.pixels) { p.x -= left; p.y -= top; }
        art = std::move(next); ++Cache.revision; return true;
    }
    bool SetProfile(const std::wstring& name) {
        if (!ValidProfile(name))return false;
        if (name == profile) { if (!profileLoaded)return false; return Starter() && Save(); }
        if (!profile.empty() && !Save())return false;
        profile = name; profileLoaded = false; Cache.drag = {}; Cache.items = {}; Cache.equipment = {}; Cache.unlocks.clear();
        seenGrass = seenStraw = heldGrass = heldStraw = 0;
        dirty = false; profileLoaded = Load(); SelectTool();
        if (!profileLoaded) { Cache.error = L"Inventory save could not be loaded."; ++Cache.revision; return false; }
        const bool success = Starter() && Save();
        if (!success)Cache.error = L"Starter sickle could not be saved.";
        ++Cache.revision; return success;
    }
    bool OpenBoth() {
        if (!Cache.ready)return false;
        Cache.inventorySign = Polarity::Plus; Cache.gearSign = Polarity::Plus;
        Cache.panelsVisible = true; ++Cache.revision; return true;
    }
    bool CloseBoth() {
        Cache.inventorySign = Polarity::Minus; Cache.gearSign = Polarity::Minus;
        Cache.panelsVisible = false; Cache.drag = {}; ++Cache.revision; return true;
    }
    bool ReceiveHarvest(std::uint32_t totalGrass, std::uint32_t totalStraw) {
        if (profile.empty() || !profileLoaded)return true;
        if (totalGrass < seenGrass)seenGrass = 0;
        if (totalStraw < seenStraw)seenStraw = 0;
        const auto g = totalGrass - seenGrass, s = totalStraw - seenStraw;
        seenGrass = totalGrass; seenStraw = totalStraw;
        if (!g && !s)return true;
        Add(Kind::Grass, g); Add(Kind::Straw, s);
        ++Cache.revision; return Save();
    }
    bool MouseDown(int x, int y) {
        if (!Cache.panelsVisible || Cache.drag.active)return false;
        int slot = Hit(x, y, false); bool gear = false;
        if (slot < 0) { slot = Hit(x, y, true); gear = true; }
        if (slot < 0)return false;
        const Item item = gear ? Cache.equipment[slot] : Cache.items[slot];
        if (item.kind == Kind::Empty)return false;
        Cache.drag = { true,gear,unsigned(slot),x,y,item };
        ++Cache.revision; return true;
    }
    bool MouseMove(int x, int y) {
        if (!Cache.drag.active)return false;
        Cache.drag.x = x; Cache.drag.y = y; ++Cache.revision; return true;
    }
    bool MouseUp(int x, int y) {
        if (!Cache.drag.active)return false;
        const auto drag = Cache.drag; Cache.drag = {};
        int slot = Hit(x, y, false); bool gear = false;
        if (slot < 0) { slot = Hit(x, y, true); gear = true; }
        if (slot < 0) { ++Cache.revision; return true; }
        auto& origin = drag.fromGear ? Cache.equipment[drag.source] : Cache.items[drag.source];
        if (drag.fromGear == gear && drag.source == unsigned(slot)) {
            ++Cache.revision; return true;
        }
        auto& dest = gear ? Cache.equipment[slot] : Cache.items[slot];
        if (origin.kind != Kind::Empty && Put(origin, dest, origin, gear, drag.fromGear, slot)) {
            dirty = true; SelectTool(); FlushOverflow(); Starter();
            if (!Save())Cache.error = L"Inventory change could not be saved.";
        }
        ++Cache.revision; return true;
    }
    bool RightDelete(int x, int y) {
        if (!Cache.panelsVisible || Cache.drag.active)return false;
        const int slot = Hit(x, y, false);
        if (slot < 0)return false;
        if (Cache.items[slot].kind == Kind::Empty)return false;
        // Right-click removes this ONE complete stack; equipment in Gear isn't deleted.
        Clear(Cache.items[slot]); FlushOverflow(); Starter(); dirty = true; ++Cache.revision;
        return Save();
    }
    bool RegisterEquipment(std::uint32_t id, std::uint32_t gearTarget) {
        if (!profileLoaded || !id || gearTarget >= Cache.equipment.size())return false;
        if (id == SAO_Stone_Sickle_Conductor::ITEM_ID && gearTarget != SAO_Stone_Sickle_Conductor::TOOL_SLOT)return false;
        Item item{}; item.kind = Kind::Equipment; item.amount = 1;
        item.equipmentId = id; item.gearSlot = gearTarget;
        for (auto& slot : Cache.items)if (slot.kind == Kind::Empty) {
            slot = item; dirty = true; ++Cache.revision; return Save();
        }
        return false;
    }
    bool SetUnlocked(const std::wstring& name, bool unlocked) {
        if (name.empty() || name.size() > 120)return false;
        auto it = std::find(Cache.unlocks.begin(), Cache.unlocks.end(), name);
        if (unlocked && it == Cache.unlocks.end())Cache.unlocks.push_back(name);
        else if (!unlocked && it != Cache.unlocks.end())Cache.unlocks.erase(it);
        else return true;
        dirty = true; ++Cache.revision; return Save();
    }
    bool IsUnlocked(const std::wstring& name) { return std::find(Cache.unlocks.begin(), Cache.unlocks.end(), name) != Cache.unlocks.end(); }
    bool Save() {
        if (!dirty)return true;
        if (profile.empty() || !profileLoaded)return false;
        const fs::path folder = Saves(); if (folder.empty())return false;
        std::error_code ec; fs::create_directories(folder, ec);
        if (ec)return false;
        const fs::path path = SavePath(), temporary = path.wstring() + L".tmp";
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)return false;
        out.write(HEADER, 8);
        if (!Write(out, VERSION))return false;
        for (const auto& s : Cache.items)if (!WriteItem(out, s))return false;
        for (const auto& s : Cache.equipment)if (!WriteItem(out, s))return false;
        if (!Write(out, heldGrass) || !Write(out, heldStraw) ||
            !Write(out, seenGrass) || !Write(out, seenStraw))return false;
        const std::uint32_t number = std::uint32_t(Cache.unlocks.size());
        if (number > 4096 || !Write(out, number))return false;
        for (const auto& s : Cache.unlocks) {
            if (s.size() > 120)return false;
            const std::uint32_t length = std::uint32_t(s.size());
            if (!Write(out, length))return false;
            for (auto c : s) {
                const std::uint16_t encoded = std::uint16_t(c);
                if (!Write(out, encoded))return false;
            }
        }
        out.flush(); if (!out)return false; out.close();
        if (fs::exists(path, ec)) {
            const fs::path backup = path.wstring() + L".bak";
            fs::copy_file(path, backup, fs::copy_options::overwrite_existing, ec);
            if (ec)return false;
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))return false;
        dirty = false; return true;
    }
    bool Load() {
        if (profile.empty())return false;
        const fs::path path = SavePath();
        std::error_code ec;
        if (!fs::exists(path, ec))return true;
        std::ifstream in(path, std::ios::binary);
        if (!in)return false;
        char header[8]{}; in.read(header, 8);
        std::uint32_t version = 0;
        if (std::memcmp(header, HEADER, 8) != 0 || !Read(in, version) || version != VERSION)return false;
        OutputNode loaded{};
        for (auto& s : loaded.items)if (!ReadItem(in, s))return false;
        for (auto& s : loaded.equipment)if (!ReadItem(in, s))return false;
        std::uint32_t overflowG = 0, overflowS = 0, priorG = 0, priorS = 0;
        if (!Read(in, overflowG) || !Read(in, overflowS) || !Read(in, priorG) || !Read(in, priorS))return false;
        std::uint32_t unlockCount = 0;
        if (!Read(in, unlockCount) || unlockCount > 4096)return false;
        for (std::uint32_t i = 0; i < unlockCount; ++i) {
            std::uint32_t length = 0; if (!Read(in, length) || length > 120)return false;
            std::wstring s; for (std::uint32_t j = 0; j < length; ++j) {
                std::uint16_t c = 0; if (!Read(in, c))return false; s.push_back(wchar_t(c));
            }
            loaded.unlocks.push_back(s);
        }
        if (in.peek() != EOF)return false;
        Cache.items = loaded.items; Cache.equipment = loaded.equipment;
        Cache.unlocks = std::move(loaded.unlocks);
        heldGrass = overflowG; heldStraw = overflowS;
        // Harvest counters restart at zero each runtime; saved stacks remain the authority.
        seenGrass = 0; seenStraw = 0;
        dirty = false; SelectTool(); return true;
    }
    void Stop() { Save(); Cache.drag = {}; }
}