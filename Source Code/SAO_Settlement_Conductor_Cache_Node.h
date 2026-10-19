#pragma once
#include "SAO_Settlement_Conductor.h"

namespace SAO_Settlement_Conductor_Cache_Node
{
    using OutputNode = SAO_Settlement_Conductor::OutputNode;
    bool Build(const OutputNode& input, HWND target);
}
