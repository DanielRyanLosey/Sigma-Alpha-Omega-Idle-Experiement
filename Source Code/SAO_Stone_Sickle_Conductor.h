#pragma once
#include "SAO_Stone_Sickle_Math.h"
#include <cstdint>
#include <vector>
namespace SAO_Stone_Sickle_Conductor {
    constexpr std::uint32_t ITEM_ID = 1;
    constexpr unsigned TOOL_SLOT = 8;
    enum class Polarity :int { Plus = 1, Minus = -1 };
    struct Pixel { UINT x = 0, y = 0; std::array<BYTE, 4> bgra{}; };
    struct BitmapGroup {
        const SAO_Static_PNG_Cache::BitmapNode* bitmap = nullptr;
        std::vector<Pixel> pixels;
        UINT width = 0, height = 0;
        int left = 0, top = 0, right = -1, bottom = -1;
    };
    struct StepNode {
        BitmapGroup group;
        unsigned next = 0;
        Polarity sign = Polarity::Plus;
    };
    struct OutputNode {
        std::array<StepNode, SAO_Stone_Sickle_Math::COUNT> steps{};
        BitmapGroup item;
        bool ready = false;
        std::wstring error;
    };
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Stone_Sickle_Math::OutputNode& source);
}
