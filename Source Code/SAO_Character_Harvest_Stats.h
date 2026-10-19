#pragma once
#include "SAO_Character_Harvest_Types.h"

namespace SAO_Character_Harvest_Stats
{
    struct OutputNode
    {
        std::array<const SAO_Character_Harvest::PieceNode*, SAO_Character_Harvest::PARTS> parts{};
        SAO_Character_Harvest::StatNode total{};
        double baseClickDamage = 1.0;
        double baseDamage = 1.0;
        bool ready = false;
        bool complete = false;
    };
    extern OutputNode Cache;
    void Attach(HWND target);
    const OutputNode& Build(const SAO_Character_Harvest::PieceNode& piece);
}
