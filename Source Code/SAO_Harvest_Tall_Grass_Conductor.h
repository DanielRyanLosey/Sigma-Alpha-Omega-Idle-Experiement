#pragma once
#include "SAO_Harvest_Tall_Grass_Math.h"
#include <atomic>
#include <array>
#include <cstdint>

namespace SAO_Harvest_Tall_Grass_Conductor
{
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };
    struct GroupNode
    {
        const SAO_Harvest_Tall_Grass_Math::BitmapNode* scene = nullptr;
        const SAO_Harvest_Tall_Grass_Math::BitmapNode* grass = nullptr;
        const SAO_Harvest_Tall_Grass_Math::BitmapNode* straw = nullptr;
        Polarity sceneSign = Polarity::Plus;
        Polarity grassSign = Polarity::Plus;
        Polarity strawSign = Polarity::Plus;
    };
    struct StepNode
    {
        SAO_Harvest_Tall_Grass_Math::BitmapNode frame;
        GroupNode group;
        UINT next = 0;
    };
    struct HitNode
    {
        // Prepared once in the Conductor; downstream reuses ONE stage overlay.
        SAO_Harvest_Tall_Grass_Math::BitmapNode overlay;
        UINT damagePercent = 0;
        UINT remainingPercent = 100;
        UINT grassReward = 0; // Numeric reward, no item PNG added to the scene.
        UINT strawReward = 0;
        const SAO_Harvest_Tall_Grass_Math::BitmapNode* grass = nullptr;
        const SAO_Harvest_Tall_Grass_Math::BitmapNode* straw = nullptr;
        // +Grass[0..4] +Straw[0..4] at 100%.
        // One pointer becomes null per 10% reduction, no PNGs are regenerated.
        std::array<const SAO_Harvest_Tall_Grass_Math::BitmapNode*, 5> grassLayers{};
        std::array<const SAO_Harvest_Tall_Grass_Math::BitmapNode*, 5> strawLayers{};
        std::array<Polarity, 5> grassSigns{};
        std::array<Polarity, 5> strawSigns{};
    };
    struct OutputNode
    {
        StepNode steps[SAO_Harvest_Tall_Grass_Math::COUNT];
        HitNode hit[11]; // 100%, 90%, ... 0%; Straw/Grass alternating.
        const SAO_Harvest_Tall_Grass_Math::BitmapNode* grassItem = nullptr;
        const SAO_Harvest_Tall_Grass_Math::BitmapNode* strawItem = nullptr;
        std::wstring error, preload_error;
        bool ready = false, animation_ready = false;
        std::atomic<bool> complete{ false };
    };
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Harvest_Tall_Grass_Math::OutputNode& input, HWND target);
}
