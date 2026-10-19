#include "SAO_Progression_Math.h"
namespace SAO_Progression_Math {
    OutputNode Build(const State& state, const BaseStats& base) {
        OutputNode result;
        result.base = base; result.harvestingLevel = state.harvestingLevel;
        result.nextHarvestXP = 100 * UNIT + state.harvestingLevel;
        result.characterLevel = state.subcategoryLevels / 100;
        result.characterPercent = state.subcategoryLevels % 100;
        result.harvestingYield = double(state.harvestingLevel) / UNIT;
        result.damage = base.damage + double(result.characterLevel) / 100;
        result.clickDamage = base.clickDamage + double(result.characterLevel) / 100;
        return result;
    }
    void CompletedHarvest(State& state) {
        state.harvestingXP += UNIT;
        while (state.harvestingXP >= 100 * UNIT + state.harvestingLevel) {
            state.harvestingXP -= 100 * UNIT + state.harvestingLevel;
            ++state.harvestingLevel; ++state.subcategoryLevels;
        }
    }
    std::uint32_t HarvestYield(std::uint32_t base, std::uint64_t level, std::uint64_t& fraction) {
        if (!base)return 0;
        // Each harvested bundle adds the flat per-level bonus to its resource reward.
        const auto total = std::uint64_t(base) * UNIT + level + fraction;
        fraction = total % UNIT;
        return static_cast<std::uint32_t>(total / UNIT);
    }
}