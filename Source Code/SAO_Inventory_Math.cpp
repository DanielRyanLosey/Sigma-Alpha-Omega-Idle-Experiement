#include "SAO_Inventory_Math.h"
#include <filesystem>
#include <windows.h>
namespace SAO_Inventory_Math {
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
                fs::path test = root / L"SAO_I_E Assets" / L"Inventory";
                std::error_code ec;
                if (fs::is_directory(test, ec))return test;
                fs::path parent = root.parent_path();
                if (parent == root)break;
                root = parent;
            }
            return {};
        }
        constexpr const wchar_t* names[COUNT] = {
        L"SAO_Inventory_Location_1.png",
        L"SAO_Inventory_Location_2.png",
        L"SAO_Inventory_Location_3.png",
        L"SAO_Inventory_Location_4.png",
        L"SAO_Inventory_Location_5.png",
        L"SAO_Inventory_Location_6.png",
        L"SAO_Inventory_Location_7.png",
        L"SAO_Inventory_Location_8.png",
        L"SAO_Inventory_Location_9.png",
        L"SAO_Inventory_Location_10.png",
        L"SAO_Inventory_Location_11.png",
        L"SAO_Inventory_Location_12.png",
        L"SAO_Inventory_Location_13.png",
        L"SAO_Inventory_Location_14.png",
        L"SAO_Inventory_Location_15.png",
        L"SAO_Inventory_Location_16.png"
        };
    }
    const OutputNode& Build() {
        if (Cache.ready)return Cache;
        const fs::path assets = AssetFolder();
        if (assets.empty()) { Cache.error = L"Inventory asset folder not found"; return Cache; }
        const fs::path folder = ExecutableFolder() / L"Cache" / L"SAO_Inventory_Math";
        Cache.window.name = L"SAO_Inventory_Window.png";
        if (!SAO_Static_PNG_Cache::Cache(assets.wstring(), folder.wstring(), Cache.window)) {
            Cache.error = L"Inventory window PNG/cache failed"; return Cache;
        }
        for (unsigned i = 0; i < COUNT; ++i) {
            auto& item = Cache.locations[i]; item.name = names[i];
            if (!SAO_Static_PNG_Cache::Cache(assets.wstring(), folder.wstring(), item)) {
                Cache.error = L"Inventory location PNG/cache failed: " + item.name; return Cache;
            }
        }
        Cache.ready = true; Cache.error.clear(); return Cache;
    }
}