#include "SAO_Game_Conductor_Cache_Node.h"
#include "SAO_Prime_Conductor.h"
#include <mutex>

namespace SAO_Game_Conductor_Cache_Node
{
    OutputNode Cache{};
    namespace
    {
        std::mutex gate;
        bool mainArrived = false;

        bool Forward(HWND target)
        {
            if (!mainArrived) return true;
            return SAO_Prime_Conductor::Build(Cache, target);
        }
    }

    bool Build(const SAO_Main_Screen_Conductor_Cache_Node::OutputNode& main, HWND target)
    {
        std::lock_guard<std::mutex> lock(gate);
        mainArrived = true;
        Cache.group.main = &main;
        Cache.input = main.input;
        Cache.window = main.window;
        Cache.symbols = main.symbols;
        Cache.scene = main.scene;
        Cache.fractal = main.fractal;
        Cache.particle = main.particle;
        Cache.bitmap = main.bitmap;
        Cache.pixels = main.pixels;
        Cache.error = main.error;
        Cache.preload_error = main.preload_error;
        Cache.ready = main.ready;
        Cache.animation_ready = main.animation_ready;
        for (UINT i = 0; i < SAO_Main_Screen_Conductor::FRAMES; ++i)
            Cache.steps[i] = main.steps[i];
        Cache.complete.store(main.complete.load(std::memory_order_acquire),
            std::memory_order_release);
        return Forward(target);
    }

    bool Build(const SAO_Settlement_Conductor_Cache_Node::OutputNode& settlement, HWND target)
    {
        std::lock_guard<std::mutex> lock(gate);
        Cache.group.settlement.store(&settlement, std::memory_order_release);
        return Forward(target);
    }
    bool Build(const SAO_Harvest_Tall_Grass_Conductor_Cache_Node::OutputNode& harvest, HWND target)
    {
        std::lock_guard<std::mutex> lock(gate);
        Cache.group.harvest.store(&harvest, std::memory_order_release);
        return Forward(target);
    }
    bool Build(const SAO_Character_Prime_Conductor_Cache_Node::OutputNode& character, HWND target)
    {
        std::lock_guard<std::mutex> lock(gate);
        Cache.group.character.store(&character, std::memory_order_release);
        return Forward(target);
    }

}