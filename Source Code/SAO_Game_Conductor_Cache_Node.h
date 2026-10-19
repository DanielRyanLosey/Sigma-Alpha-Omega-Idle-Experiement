#pragma once
#include "SAO_Main_Screen_Conductor_Cache_Node.h"
#include "SAO_Settlement_Conductor_Cache_Node.h"
#include "SAO_Harvest_Tall_Grass_Conductor_Cache_Node.h"
#include "SAO_Character_Prime_Conductor_Cache_Node.h"
#include <cstdint>
#include <atomic>

namespace SAO_Game_Conductor_Cache_Node
{
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };
    struct GroupNode
    {
        const SAO_Main_Screen_Conductor_Cache_Node::OutputNode* main = nullptr;
        std::atomic<const SAO_Settlement_Conductor_Cache_Node::OutputNode*> settlement{ nullptr };
        std::atomic<const SAO_Harvest_Tall_Grass_Conductor_Cache_Node::OutputNode*> harvest{ nullptr };
        std::atomic<const SAO_Character_Prime_Conductor_Cache_Node::OutputNode*> character{ nullptr };
        Polarity mainAtMenu = Polarity::Plus;
        Polarity settlementAtMenu = Polarity::Minus;
        Polarity mainAtSettlement = Polarity::Minus;
        Polarity settlementAtSettlement = Polarity::Plus;
        Polarity settlementAtHarvest = Polarity::Minus;
        Polarity harvestAtHarvest = Polarity::Plus;
    };
    struct OutputNode : SAO_Main_Screen_Conductor_Cache_Node::OutputNode
    {
        GroupNode group;
    };
    extern OutputNode Cache;
    bool Build(const SAO_Main_Screen_Conductor_Cache_Node::OutputNode& main, HWND target);
    bool Build(const SAO_Settlement_Conductor_Cache_Node::OutputNode& settlement, HWND target);
    bool Build(const SAO_Harvest_Tall_Grass_Conductor_Cache_Node::OutputNode& harvest, HWND target);
    bool Build(const SAO_Character_Prime_Conductor_Cache_Node::OutputNode& character, HWND target);
}
