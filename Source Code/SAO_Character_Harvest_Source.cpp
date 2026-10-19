#include "SAO_Character_Harvest_Source.h"
#include "SAO_Character_Harvest_Stats.h"
#include "SAO_Base_Harvest_Action_Feet_Math.h"
#include "SAO_Base_Harvest_Action_Head_Math.h"
#include "SAO_Base_Harvest_Action_Legs_Math.h"
#include "SAO_Base_Harvest_Action_LFA_Math.h"
#include "SAO_Base_Harvest_Action_LH_Math.h"
#include "SAO_Base_Harvest_Action_LUA_Math.h"
#include "SAO_Base_Harvest_Action_RFA_Math.h"
#include "SAO_Base_Harvest_Action_RH_Math.h"
#include "SAO_Base_Harvest_Action_RUA_Math.h"
#include "SAO_Base_Harvest_Action_Tool_Math.h"
#include "SAO_Base_Harvest_Action_Torso_Math.h"
#include <process.h>

namespace SAO_Character_Harvest_Source
{
    namespace
    {
        HANDLE worker = nullptr;
        HANDLE preload = nullptr;
        HANDLE cancel = nullptr;
        unsigned __stdcall Emit(void*)
        {
            // One worker, eleven independent source nodes: minimal peak RAM.
            SAO_Base_Harvest_Action_Feet_Math::Build(false);
            SAO_Base_Harvest_Action_Head_Math::Build(false);
            SAO_Base_Harvest_Action_Legs_Math::Build(false);
            SAO_Base_Harvest_Action_LFA_Math::Build(false);
            SAO_Base_Harvest_Action_LH_Math::Build(false);
            SAO_Base_Harvest_Action_LUA_Math::Build(false);
            SAO_Base_Harvest_Action_RFA_Math::Build(false);
            SAO_Base_Harvest_Action_RH_Math::Build(false);
            SAO_Base_Harvest_Action_RUA_Math::Build(false);
            SAO_Base_Harvest_Action_Tool_Math::Build(false);
            SAO_Base_Harvest_Action_Torso_Math::Build(false);
            const HANDLE events[]{ cancel, preload };
            if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                return 0;
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_Feet_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_Head_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_Legs_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_LFA_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_LH_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_LUA_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_RFA_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_RH_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_RUA_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_Tool_Math::Build(true);
            if (WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) return 0;
            SAO_Base_Harvest_Action_Torso_Math::Build(true);
            return 0;
        }
    }
    bool Start(HWND catcher)
    {
        if (worker) return true;
        SAO_Character_Harvest_Stats::Attach(catcher);
        cancel = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        preload = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (cancel && preload)
            worker = reinterpret_cast<HANDLE>(
                _beginthreadex(nullptr, 0, Emit, nullptr, 0, nullptr));
        if (worker) return true;
        if (cancel) CloseHandle(cancel);
        if (preload) CloseHandle(preload);
        cancel = preload = nullptr;
        return false;
    }
    void Preload() { if (preload) SetEvent(preload); }
    void Stop()
    {
        if (cancel) SetEvent(cancel);
        if (worker) { WaitForSingleObject(worker, INFINITE); CloseHandle(worker); }
        if (cancel) CloseHandle(cancel);
        if (preload) CloseHandle(preload);
        worker = cancel = preload = nullptr;
    }
}