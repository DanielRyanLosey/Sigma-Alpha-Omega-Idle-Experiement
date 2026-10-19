#pragma once
#include "SAO_Main_Screen_Conductor.h"

namespace SAO_Main_Screen_Conductor_Cache_Node
{
    using OutputNode = SAO_Main_Screen_Conductor::OutputNode;
    bool Build(const OutputNode& input, HWND target);
}
