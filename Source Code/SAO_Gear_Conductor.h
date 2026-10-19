#pragma once
#include "SAO_Gear_Math.h"
#include <array>
namespace SAO_Gear_Conductor {
    enum class Polarity :int { Plus = 1, Minus = -1 };
    struct GroupNode {
        const SAO_Static_PNG_Cache::BitmapNode* window = nullptr;
        std::array<const SAO_Static_PNG_Cache::BitmapNode*, SAO_Gear_Math::COUNT> locations{};
        std::array<SAO_Static_PNG_Cache::Anchor, SAO_Gear_Math::COUNT> anchors{};
        Polarity windowSign = Polarity::Plus;
    };
    struct OutputNode { GroupNode group{}; bool ready = false; std::wstring error; };
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Gear_Math::OutputNode& source);
}
