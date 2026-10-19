#pragma once
#include <windows.h>
#include <string>
#include <cstdint>
#include <vector>

namespace SAO_Static_PNG_Cache {
    struct BitmapNode {
        std::wstring name, path;
        UINT width = 0, height = 0;
        bool ready = false;
    };
    struct Anchor { int x = 0, y = 0; bool ready = false; };
    // Source bitmap -> raw BGRA cache. No shared full-frame buffers retained.
    bool Cache(const std::wstring& sourceFolder, const std::wstring& cacheFolder,
        BitmapNode& node);
    // Sparse location marks -> one numerical anchor; the bitmap is not kept in RAM.
    Anchor Locate(const BitmapNode& node);
    bool Load(const BitmapNode& node, std::vector<BYTE>& pixels);
}
