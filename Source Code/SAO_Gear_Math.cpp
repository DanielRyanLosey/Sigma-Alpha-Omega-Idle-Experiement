#include "SAO_Gear_Math.h"
#include <filesystem>
#include <windows.h>
namespace SAO_Gear_Math {
    OutputNode Cache{};
    namespace {
        namespace fs = std::filesystem;
        fs::path ExecutableFolder() {
            wchar_t path[MAX_PATH]{};
            DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
            return (!n || n >= MAX_PATH) ? fs::path{} : fs::path(std::wstring(path, n)).parent_path();
        }
        fs::path AssetFolder() {
            fs::path root = ExecutableFolder();
            for (int i = 0; i < 8 && !root.empty(); ++i) {
                fs::path test = root / L"SAO_I_E Assets" / L"Gear";
                std::error_code ec;
                if (fs::is_directory(test, ec))return test;
                fs::path parent = root.parent_path();
                if (parent == root)break;
                root = parent;
            }
            return {};
        }
        constexpr const wchar_t* names[COUNT] = {
        L"SAO_Gear_Boots_Location.png",
        L"SAO_Gear_Bracers_Location.png",
        L"SAO_Gear_Hands_Location.png",
        L"SAO_Gear_Head_Location.png",
        L"SAO_Gear_Legs_Location.png",
        L"SAO_Gear_Necklace_Location.png",
        L"SAO_Gear_Ring_Location.png",
        L"SAO_Gear_Shoulders_Location.png",
        L"SAO_Gear_Tool_Location.png",
        L"SAO_Gear_Torso_Location.png",
        L"SAO_Gear_Trinket_1_Location.png",
        L"SAO_Gear_Trinket_2_Location.png",
        L"SAO_Gear_Weapon_Location.png"
        };
    }
    const OutputNode& Build() {
        if (Cache.ready)return Cache;
        const fs::path assets = AssetFolder();
        if (assets.empty()) { Cache.error = L"Gear asset folder not found"; return Cache; }
        const fs::path folder = ExecutableFolder() / L"Cache" / L"SAO_Gear_Math";
        Cache.window.name = L"SAO_Gear_Window.png";
        if (!SAO_Static_PNG_Cache::Cache(assets.wstring(), folder.wstring(), Cache.window)) {
            Cache.error = L"Gear window PNG/cache failed"; return Cache;
        }
        for (unsigned i = 0; i < COUNT; ++i) {
            auto& item = Cache.locations[i]; item.name = names[i];
            if (!SAO_Static_PNG_Cache::Cache(assets.wstring(), folder.wstring(), item)) {
                Cache.error = L"Gear location PNG/cache failed: " + item.name; return Cache;
            }
        }
        Cache.ready = true; Cache.error.clear(); return Cache;
    }
}