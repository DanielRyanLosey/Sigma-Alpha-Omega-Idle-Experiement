#pragma once
#include "SAO_Static_PNG_Cache.h"
#include <array>
namespace SAO_Gear_Math {
    constexpr unsigned COUNT = 13;
    struct OutputNode {
        SAO_Static_PNG_Cache::BitmapNode window;
        std::array<SAO_Static_PNG_Cache::BitmapNode, COUNT> locations{};
        bool ready = false;
        std::wstring error;
    };
    extern OutputNode Cache;
    const OutputNode& Build(); // Static image decoding once, no timer.
}
