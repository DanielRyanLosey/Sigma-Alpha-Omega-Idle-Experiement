#include "SAO_Main_Screen_Math_Cache_Node.h"
#include "SAO_Main_Screen_Conductor.h"

SAO_Main_Screen_Math_Cache_Node::OutputNode SAO_Main_Screen_Math_Cache_Node::Cache{};

const SAO_Main_Screen_Math_Cache_Node::OutputNode& SAO_Main_Screen_Math_Cache_Node::Build(
    const SAO_Main_Screen_Math::OutputNode& screen,
    const SAO_Main_Screen_Particle_Math::OutputNode& particles,
    const SAO_Main_Title_Math::OutputNode& title,
    const SAO_Main_Buttons_Math::OutputNode& buttons, HWND target)
{
    Cache.screen = &screen; Cache.particles = &particles;
    Cache.title = &title; Cache.buttons = &buttons;
    Cache.ready = screen.ready && particles.ready && title.ready && buttons.ready;
    Cache.error = !screen.ready ? screen.error : !particles.ready ? particles.error :
        !title.ready ? title.error : buttons.error;
    SAO_Main_Screen_Conductor::Build(Cache, target);
    return Cache;
}