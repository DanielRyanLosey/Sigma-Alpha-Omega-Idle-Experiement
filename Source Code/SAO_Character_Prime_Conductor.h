#pragma once
#include "SAO_Character_Harvest_Stats.h"

namespace SAO_Character_Prime_Conductor
{
    using namespace SAO_Character_Harvest;
    struct StepNode
    {
        BitmapNode frame;
        const BitmapNode* oakTool = nullptr;
        Polarity oakSign = Polarity::Minus;
        std::array<const BitmapNode*, PARTS> layers{};
        std::array<Polarity, PARTS> signs{};
        UINT next = 0;
    };
    struct OutputNode
    {
        const SAO_Character_Harvest_Stats::OutputNode* stats = nullptr;
        std::array<StepNode, COUNT> steps{};
        std::wstring error;
        bool ready = false;
        bool complete = false;
    };
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Character_Harvest_Stats::OutputNode& input,
        HWND target);
}
