#pragma once
#include "SAO_Character_Harvest_Types.h"

namespace SAO_Base_Harvest_Action_Feet_Math
{
    using OutputNode = SAO_Character_Harvest::MathNode;
    extern OutputNode Cache;
    const OutputNode& Build(bool preload = false);
}
