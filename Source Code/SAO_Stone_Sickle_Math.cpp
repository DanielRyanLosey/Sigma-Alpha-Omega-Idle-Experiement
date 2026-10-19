#include "SAO_Stone_Sickle_Math.h"
#include <filesystem>
namespace SAO_Stone_Sickle_Math {
    OutputNode Cache{};
    const OutputNode& Build() {
        if (Cache.ready)return Cache;
        namespace fs = std::filesystem;
        wchar_t executable[MAX_PATH]{};
        const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (!n || n >= MAX_PATH) { Cache.error = L"Sickle executable path unavailable."; return Cache; }
        const auto base = fs::path(std::wstring(executable, n)).parent_path();
        fs::path root = base, folder;
        for (unsigned i = 0; i < 8 && !root.empty(); ++i) {
            const auto test = root / L"SAO_I_E Assets" / L"Gear" / L"Stone" / L"Stone Tools" / L"Stone Sickle";
            std::error_code ec;
            if (fs::is_directory(test, ec)) { folder = test; break; }
            const auto parent = root.parent_path();
            if (parent == root)break;
            root = parent;
        }
        if (folder.empty()) { Cache.error = L"Gear\\Stone\\Stone Tools\\Stone Sickle folder not found."; return Cache; }
        const auto destination = base / L"Cache" / L"SAO_Stone_Sickle_Math";
        constexpr const wchar_t* names[COUNT] = {
            L"SAO_Stone_Tools_Sickle_0001.png",L"SAO_Stone_Tools_Sickle_0002.png",
            L"SAO_Stone_Tools_Sickle_0003.png",L"SAO_Stone_Tools_Sickle_0004.png",
            L"SAO_Stone_Tools_Sickle_0005.png",L"SAO_Stone_Tools_Sickle_0006.png",
            L"SAO_Stone_Tools_Sickle_0007.png",L"SAO_Stone_Tools_Sickle_0008.png",
            L"SAO_Stone_Tools_Sickle_0009.png",L"SAO_Stone_Tools_Sickle_0010.png",
            L"SAO_Stone_Tools_Sickle_0011.png",L"SAO_Stone_Tools_Sickle_0012.png",
            L"SAO_Stone_Tools_Sickle_0013.png",L"SAO_Stone_Tools_Sickle_0014.png",
            L"SAO_Stone_Tools_Sickle_0015.png",L"SAO_Stone_Tools_Sickle_0016.png",
            L"SAO_Stone_Tools_Sickle_0017.png",L"SAO_Stone_Tools_Sickle_0018.png",
            L"SAO_Stone_Tools_Sickle_0019.png",L"SAO_Stone_Tools_Sickle_0020.png",
            L"SAO_Stone_Tools_Sickle_0021.png",L"SAO_Stone_Tools_Sickle_0022.png",
            L"SAO_Stone_Tools_Sickle_0023.png"
        };
        for (unsigned i = 0; i < COUNT; ++i) {
            auto& frame = Cache.frames[i]; frame.name = names[i];
            if (!SAO_Static_PNG_Cache::Cache(folder.wstring(), destination.wstring(), frame)) {
                Cache.error = L"Sickle PNG/cache failed: " + frame.name; return Cache;
            }
        }
        Cache.item.name = L"SAO_Stone_Tools_Sickle_Item.png";
        if (!SAO_Static_PNG_Cache::Cache(folder.wstring(), destination.wstring(), Cache.item)) {
            Cache.error = L"Sickle item PNG/cache failed: " + Cache.item.name; return Cache;
        }
        Cache.error.clear(); Cache.ready = true; return Cache;
    }
}