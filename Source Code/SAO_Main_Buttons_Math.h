#pragma once
#include "SAO_Main_Screen_Math.h"

namespace SAO_Main_Buttons_Math
{
    constexpr UINT COUNT = 3;
    struct OutputNode
    {
        SAO_Bitmap_Cache_Node layers[COUNT];
        std::wstring error;
        bool ready = false;
    };
    extern OutputNode Cache;
    const OutputNode& Build();
}

