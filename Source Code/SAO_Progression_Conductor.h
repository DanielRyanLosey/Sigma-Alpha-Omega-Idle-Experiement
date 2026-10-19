#pragma once
#include "SAO_Progression_Math.h"
#include <string>
namespace SAO_Progression_Conductor {
    struct Rewards { std::uint32_t grass = 0, straw = 0; };
    struct OutputNode {
        SAO_Progression_Math::State state;
        SAO_Progression_Math::OutputNode stats;
        std::wstring player, characterText, harvestingText, error;
        std::uint64_t revision = 0;
        bool ready = false;
    };
    extern OutputNode Cache;
    bool SetProfile(const std::wstring& name);
    void ReceiveBase(const SAO_Progression_Math::BaseStats& base);
    Rewards ReceiveHarvest(std::uint32_t grass, std::uint32_t straw, bool completed);
    void Rollback(const SAO_Progression_Math::State& state);
    bool Save();
}
