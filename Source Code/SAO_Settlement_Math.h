#pragma once
#include <windows.h>
#include <atomic>
#include <string>

namespace SAO_Settlement_Math
{
    constexpr UINT COUNT = 30;
    struct BitmapNode
    {
        std::wstring name, path;
        bool ready = false;
    };
    struct OutputNode
    {
        BitmapNode scenes[COUNT];
        BitmapNode harvesting;
        BitmapNode back;
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
