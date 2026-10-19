#pragma once
#include "SAO_Base_Harvest_Action_Tool_Math.h"

namespace SAO_Base_Harvest_Action_Tool_Conductor
{
    using OutputNode = SAO_Character_Harvest::PieceNode;
    extern OutputNode Cache;
    const OutputNode& Build(const SAO_Base_Harvest_Action_Tool_Math::OutputNode& input);
}
