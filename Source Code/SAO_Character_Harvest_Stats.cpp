#include "SAO_Character_Harvest_Stats.h"
#include "SAO_Character_Prime_Conductor.h"

namespace SAO_Character_Harvest_Stats
{
    OutputNode Cache{};
    namespace { HWND destination = nullptr; }
    void Attach(HWND target) { destination = target; }

    const OutputNode& Build(const SAO_Character_Harvest::PieceNode& piece)
    {
        const UINT id = static_cast<UINT>(piece.part);
        if (id >= SAO_Character_Harvest::PARTS) return Cache;
        Cache.parts[id] = &piece;

        bool allReady = true, allComplete = true;
        for (const auto* part : Cache.parts)
        {
            if (!part || !part->ready) allReady = false;
            if (!part || !part->complete) allComplete = false;
        }
        if (!allReady) return Cache;

        // The eleven ones produce ONE, not eleven, points of base damage.
        SAO_Character_Harvest::StatNode sum{};
        sum.damage = sum.strength = sum.dexterity = 0;
        sum.endurance = sum.precision = 0;
        for (const auto* part : Cache.parts)
        {
            sum.damage += part->stats.damage;
            sum.strength += part->stats.strength;
            sum.dexterity += part->stats.dexterity;
            sum.endurance += part->stats.endurance;
            sum.precision += part->stats.precision;
        }
        constexpr double count = static_cast<double>(SAO_Character_Harvest::PARTS);
        Cache.total.damage = sum.damage / count;
        Cache.total.strength = sum.strength / count;
        Cache.total.dexterity = sum.dexterity / count;
        Cache.total.endurance = sum.endurance / count;
        Cache.total.precision = sum.precision / count;
        Cache.baseClickDamage = 1.0;
        Cache.baseDamage = Cache.total.damage;
        Cache.ready = true;
        Cache.complete = allComplete;

        SAO_Character_Prime_Conductor::Build(Cache, destination);
        return Cache;
    }
}