#pragma once
#include "SAO_Main_Screen_Math.h"
#include "SAO_Main_Screen_Particle_Math.h"
#include "SAO_Main_Title_Math.h"
#include "SAO_Main_Buttons_Math.h"

namespace SAO_Main_Screen_Math_Cache_Node
{
    struct OutputNode
    {
        const SAO_Main_Screen_Math::OutputNode* screen = nullptr;
        const SAO_Main_Screen_Particle_Math::OutputNode* particles = nullptr;
        const SAO_Main_Title_Math::OutputNode* title = nullptr;
        const SAO_Main_Buttons_Math::OutputNode* buttons = nullptr;
        std::wstring error;
        bool ready = false;
    };
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Main_Screen_Math::OutputNode& screen,
        const SAO_Main_Screen_Particle_Math::OutputNode& particles,
        const SAO_Main_Title_Math::OutputNode& title,
        const SAO_Main_Buttons_Math::OutputNode& buttons, HWND target);
}
