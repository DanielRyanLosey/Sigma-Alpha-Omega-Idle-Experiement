#pragma once
#include "SAO_Static_PNG_Cache.h"
#include <array>
namespace SAO_Stone_Sickle_Math {
    constexpr unsigned COUNT = 23;
    struct OutputNode {
        std::array<SAO_Static_PNG_Cache::BitmapNode, COUNT> frames{};
        SAO_Static_PNG_Cache::BitmapNode item;
        bool ready = false;
        std::wstring error;
    };
    extern OutputNode Cache;
    const OutputNode& Build();
}
