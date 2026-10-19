#pragma once
#include "SAO_Base_Harvest_Action_RH_Math.h"

namespace SAO_Base_Harvest_Action_RH_Conductor
{
    using OutputNode = SAO_Character_Harvest::PieceNode;
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Base_Harvest_Action_RH_Math::OutputNode& input);
}
