#pragma once
#include "SAO_Settlement_Math.h"
#include <atomic>
#include <cstdint>

namespace SAO_Settlement_Conductor
{
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };
    struct GroupNode
    {
        const SAO_Settlement_Math::BitmapNode* scene = nullptr;
        const SAO_Settlement_Math::BitmapNode* harvesting = nullptr;
        const SAO_Settlement_Math::BitmapNode* back = nullptr;
        Polarity sceneSign = Polarity::Plus;
        Polarity harvestingSign = Polarity::Plus;
        Polarity backSign = Polarity::Plus;
    };
    struct StepNode
    {
        SAO_Settlement_Math::BitmapNode frame;
        GroupNode group;
        UINT next = 0;
    };
    struct OutputNode
    {
        StepNode steps[SAO_Settlement_Math::COUNT];
        std::wstring error, preload_error;
        bool ready = false, animation_ready = false;
        std::atomic<bool> complete{ false };
    };
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Settlement_Math::OutputNode& input, HWND target);
}
