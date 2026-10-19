#pragma once
#include <cstdint>
namespace SAO_Progression_Math {
    constexpr std::uint64_t UNIT = 200; // 1/200 = exactly 0.005
    struct State {
        std::uint64_t harvestingLevel = 0, harvestingXP = 0;
        std::uint64_t subcategoryLevels = 0;
        std::uint64_t grassFraction = 0, strawFraction = 0;
    };
    struct BaseStats {
        double damage = 1, clickDamage = 1, strength = 1, dexterity = 1, endurance = 1, precision = 1;
    };
    struct OutputNode {
        std::uint64_t harvestingLevel = 0, characterLevel = 0;
        std::uint64_t characterPercent = 0, nextHarvestXP = 20000;
        double harvestingYield = 0, damage = 1, clickDamage = 1;
        BaseStats base;
    };
    OutputNode Build(const State& state, const BaseStats& base);
    void CompletedHarvest(State& state); // One XP for all ten bundles, not each swing.
    std::uint32_t HarvestYield(std::uint32_t base, std::uint64_t level, std::uint64_t& fraction);
}
