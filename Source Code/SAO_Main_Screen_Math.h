#pragma once
#include <windows.h>
#include <string>

struct SAO_Bitmap_Cache_Node
{
    std::wstring name, path;
    bool ready = false;
};

namespace SAO_Main_Screen_Math
{
    constexpr UINT COUNT = 25;
    struct OutputNode
    {
        SAO_Bitmap_Cache_Node layers[COUNT];
        std::wstring error;
        bool ready = false;
    };
    extern OutputNode Cache;
    const OutputNode& Build(bool preload = true);
    bool Start(HWND target);
    void Preload();
    void Stop();
    bool Cancelled();
}