#pragma once
#include <windows.h>
#include <atomic>
#include <string>

namespace SAO_Harvest_Tall_Grass_Math
{
    constexpr UINT COUNT = 45;
    struct BitmapNode
    {
        std::wstring name, path;
        UINT width = 0, height = 0;
        bool ready = false;
    };
    struct OutputNode
    {
        BitmapNode scenes[COUNT];
        BitmapNode grass[5];      // Grass, Grass1..Grass4
        BitmapNode straw[5];      // Straw, Straw1..Straw4
        BitmapNode grassItem;     // Reward source, never added to scene
        BitmapNode strawItem;     // Reward source, never added to scene
        std::wstring error;
        bool ready = false;
        std::atomic<bool> complete{ false };
    };
    extern OutputNode Cache;
    const OutputNode& Build(bool preload = true);
    bool Start(HWND target);
    void Preload();
    void Stop();
    bool Cancelled();
}
