#pragma once
#include "SAO_Main_Screen_Math.h"

namespace SAO_Main_Screen_Particle_Math
{
    constexpr UINT COUNT = 45;
    struct OutputNode
    {
        SAO_Bitmap_Cache_Node layers[COUNT];
        std::wstring error;
        bool ready = false;
    };
    extern OutputNode Cache;
    const OutputNode& Build(bool preload = true);
}



