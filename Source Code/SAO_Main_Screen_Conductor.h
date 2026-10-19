#pragma once
#include "SAO_Main_Screen_Math_Cache_Node.h"
#include <atomic>

namespace SAO_Main_Screen_Conductor
{
    constexpr UINT WIDTH = 1920, HEIGHT = 1080, BYTES = WIDTH * HEIGHT * 4;
    constexpr UINT FRAMES = 360, STEP_MS = 33; // 24 and 45 return together after 360 steps.
    struct GroupNode
    {
        const SAO_Bitmap_Cache_Node* const* layers = nullptr;
        UINT count = 0;
    };
    struct PairNode
    {
        const SAO_Bitmap_Cache_Node* left = nullptr;
        const SAO_Bitmap_Cache_Node* right = nullptr;
        UINT next = 0;
    };
    struct StepNode
    {
        SAO_Bitmap_Cache_Node frame;
        UINT next = 0;
    };
    struct OutputNode
    {
        const SAO_Main_Screen_Math_Cache_Node::OutputNode* input = nullptr;
        GroupNode window, symbols, scene;
        const PairNode* fractal = nullptr;
        const PairNode* particle = nullptr;
        StepNode steps[FRAMES];
        HBITMAP bitmap = nullptr;
        BYTE* pixels = nullptr;
        std::wstring error, preload_error;
        bool ready = false, animation_ready = false;
        std::atomic<bool> complete{ false };
    };
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Main_Screen_Math_Cache_Node::OutputNode& input, HWND target);
}
