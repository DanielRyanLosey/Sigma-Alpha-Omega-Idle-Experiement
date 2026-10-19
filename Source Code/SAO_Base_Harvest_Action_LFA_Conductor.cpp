#include "SAO_Base_Harvest_Action_LFA_Conductor.h"
#include "SAO_Character_Harvest_Stats.h"

namespace SAO_Base_Harvest_Action_LFA_Conductor
{
    OutputNode Cache{};
    const OutputNode& Build(const SAO_Base_Harvest_Action_LFA_Math::OutputNode& input)
    {
        Cache.part = SAO_Character_Harvest::Part::LFA;
        Cache.ready = input.ready;
        Cache.complete = input.complete;
        Cache.error = input.error;
        // Exactly five independent base stats, all 1 until an upgrade changes them.
        // All PNGs stay in their original 1920x1080 Blender coordinates.
        for (UINT i = 0; i < SAO_Character_Harvest::COUNT; ++i)
        {
            Cache.steps[i].frame = &input.frames[i];
            Cache.steps[i].sign = SAO_Character_Harvest::Polarity::Plus;
            Cache.steps[i].next = (i + 1) % SAO_Character_Harvest::COUNT;
        }
        SAO_Character_Harvest_Stats::Build(Cache);
        return Cache;
    }
}