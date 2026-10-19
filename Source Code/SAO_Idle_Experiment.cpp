#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include "SAO_Prime_Cache_Node.h"
#include "SAO_Inventory_Math.h"
#include "SAO_Gear_Math.h"
#include "SAO_Inventory_Conductor.h"
#include "SAO_Gear_Conductor.h"
#include "SAO_Gear_Inventory_Conductor.h"
#include "SAO_Stone_Sickle_Math.h"
#include "SAO_Stone_Sickle_Conductor.h"
#include <process.h>
#include "SAO_Settlement_Math.h"
#include "SAO_Harvest_Tall_Grass_Math.h"
#include "SAO_Character_Harvest_Source.h"
#include "SAO_Alphabet_Conductor.h"
#include "SAO_Alphabet_Startup.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")

namespace SAO_Boundary
{
    constexpr int WIDTH = 1920, HEIGHT = 1080;
    constexpr UINT_PTR PRELOAD = 1, ANIMATION = 2;

    // The program receives ONE final cache from the shared Prime cache node.
    const SAO_Prime_Cache_Node::OutputNode* projection = nullptr;
    int show_mode = SW_SHOWNORMAL;
    bool preload_scheduled = false;
    bool animation_started = false;
    UINT lastHarvestHits = 0;
    constexpr UINT INVENTORY_PRELOADED = WM_APP + 91;
    HANDLE inventoryWorker = nullptr;
    bool inventoryReady = false;
    unsigned __stdcall PreloadInventory(void* context) {
        const auto window = static_cast<HWND>(context);
        const auto& inventory = SAO_Inventory_Math::Build();
        const auto& gear = SAO_Gear_Math::Build();
        SAO_Inventory_Conductor::Build(inventory);
        SAO_Gear_Conductor::Build(gear);
        const auto& sickle = SAO_Stone_Sickle_Math::Build();
        SAO_Stone_Sickle_Conductor::Build(sickle);
        PostMessageW(window, INVENTORY_PRELOADED, 0, 0);
        return 0;
    }

    LRESULT CALLBACK Receive(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case INVENTORY_PRELOADED:
            // Synchronize completed producer output before forwarding to the consumer.
            if (inventoryWorker) {
                WaitForSingleObject(inventoryWorker, INFINITE);
                CloseHandle(inventoryWorker); inventoryWorker = nullptr;
            }
            inventoryReady = SAO_Prime_Cache_Node::ReceiveInventory(
                SAO_Inventory_Conductor::Cache, SAO_Gear_Conductor::Cache,
                SAO_Stone_Sickle_Conductor::Cache);
            if (!inventoryReady) {
                MessageBoxW(window, SAO_Gear_Inventory_Conductor::Cache.error.c_str(),
                    L"Inventory/Gear preload", MB_OK | MB_ICONERROR);
                DestroyWindow(window);
            }
            return 0;
        case SAO_Prime_Cache_Node::SAO_INPUT:
        {
            const auto& input = *reinterpret_cast<const SAO_Prime_Cache_Node::OutputNode*>(lParam);
            // Program catches the forwarded damage/reward numbers.
            // This log runs only on completed hits, never every animation tick.
            if (input.harvest.completedHits != lastHarvestHits)
            {
                lastHarvestHits = input.harvest.completedHits;
                wchar_t status[192]{};
                swprintf_s(status,
                    L"SAO Harvest | Hit %u | HP %u/%u | Bushels -%u | Straw +%u | Grass +%u\n",
                    input.harvest.completedHits, input.harvest.health,
                    input.harvest.maxHealth, input.harvest.removedBushels,
                    input.harvest.strawItems, input.harvest.grassItems);
                OutputDebugStringW(status);
            }
            if (!input.ready)
            {
                MessageBoxW(window, input.error.empty() ? L"The shared Prime cache is not ready."
                    : input.error.c_str(), L"SAO Idle", MB_OK | MB_ICONERROR);
                DestroyWindow(window);
                return 0;
            }
            if (!projection)
            {
                projection = &input;
                // BOTH caches have already been combined at this point.
                // No blank first screen, no separate Toth presentation step.
                InvalidateRect(window, nullptr, FALSE);
                ShowWindow(window, show_mode);
                UpdateWindow(window);
            }
            else
            {
                // A Toth view change has already been composed in the same DIB.
                InvalidateRect(window, nullptr, FALSE);
            }
            // Program still owns the one animation clock.
            // A selected static first frame waits for its prepared sequence.
            if (animation_started && !input.animation_ready)
            {
                KillTimer(window, ANIMATION);
                animation_started = false;
            }
            const auto* settlement = input.game ? input.game->group.settlement.load(std::memory_order_acquire) : nullptr;
            const auto* harvest = input.game ? input.game->group.harvest.load(std::memory_order_acquire) : nullptr;
            const auto* character = input.game ? input.game->group.character.load(std::memory_order_acquire) : nullptr;
            const bool bulkReady = inventoryReady && settlement && harvest && character &&
                settlement->complete.load(std::memory_order_acquire) && settlement->animation_ready &&
                harvest->complete.load(std::memory_order_acquire) && harvest->animation_ready && character->complete;
            if (bulkReady && input.game &&
                input.game->complete.load(std::memory_order_acquire) &&
                !animation_started)
            {
                if (!input.game->animation_ready)
                    MessageBoxW(window, input.game->preload_error.c_str(),
                        L"SAO Idle", MB_OK | MB_ICONERROR);
                else if (input.animation_ready)
                {
                    if (SetTimer(window, ANIMATION,
                        SAO_Main_Screen_Conductor::STEP_MS, nullptr))
                        animation_started = true;
                    else MessageBoxW(window, L"Animation timer could not start.",
                        L"SAO Idle", MB_OK | MB_ICONERROR);
                }
                // The initial Settlement projection stays static until its
                // independent 45-frame cache is ready; Toth remains unchanged.
            }
            return 0;
        }
        case WM_LBUTTONDOWN:
            SAO_Prime_Cache_Node::Click(
                static_cast<std::uint32_t>(GET_X_LPARAM(lParam)),
                static_cast<std::uint32_t>(GET_Y_LPARAM(lParam)), window);
            if (SAO_Prime_Cache_Node::InventoryDragging())SetCapture(window);
            return 0;
        case WM_MOUSEMOVE:
            if (SAO_Prime_Cache_Node::InventoryDragging())
                SAO_Prime_Cache_Node::MouseMove(
                    static_cast<std::uint32_t>(GET_X_LPARAM(lParam)),
                    static_cast<std::uint32_t>(GET_Y_LPARAM(lParam)));
            return 0;
        case WM_LBUTTONUP:
            if (SAO_Prime_Cache_Node::InventoryDragging()) {
                SAO_Prime_Cache_Node::MouseUp(
                    static_cast<std::uint32_t>(GET_X_LPARAM(lParam)),
                    static_cast<std::uint32_t>(GET_Y_LPARAM(lParam)));
                if (GetCapture() == window)ReleaseCapture();
            }
            return 0;
        case WM_RBUTTONDOWN:
            SAO_Prime_Cache_Node::RightClick(
                static_cast<std::uint32_t>(GET_X_LPARAM(lParam)),
                static_cast<std::uint32_t>(GET_Y_LPARAM(lParam)));
            return 0;
            // Toth's max Enter PNG forwards the click event here, rather than
            // calling Alphabet backwards from the visual overlay.
        case WM_APP + 9:
            SAO_Alphabet_Conductor::Submit();
            return 0;
        case WM_CHAR:
            if (wParam == L'\r')
                SAO_Alphabet_Conductor::Submit();
            else if (wParam == L'\b')
                SAO_Alphabet_Conductor::Backspace();
            else if (wParam >= 32 && wParam != 127)
                SAO_Alphabet_Conductor::Character(static_cast<wchar_t>(wParam));
            return 0;
        case WM_TIMER:
            if (wParam == PRELOAD)
            {
                KillTimer(window, PRELOAD);
                // First image is already displayed. Bulk work starts only now.
                inventoryWorker = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0,
                    PreloadInventory, window, 0, nullptr));
                if (!inventoryWorker || !SAO_Settlement_Math::Start(window) ||
                    !SAO_Harvest_Tall_Grass_Math::Start(window) ||
                    !SAO_Character_Harvest_Source::Start(window)) {
                    MessageBoxW(window, L"Bulk preload could not start.", L"SAO Idle", MB_OK | MB_ICONERROR);
                    DestroyWindow(window); return 0;
                }
                // Existing two-second one-shot preload. Program owns the clock.
                SAO_Main_Screen_Math::Preload();
                SAO_Settlement_Math::Preload();
                SAO_Harvest_Tall_Grass_Math::Preload();
                SAO_Character_Harvest_Source::Preload();
                SAO_Prime_Cache_Node::Preload();
                SAO_Alphabet_Startup::Start();
            }
            else if (wParam == ANIMATION && projection && animation_started)
            {
                // ONE final bitmap. Toth is static; only Prime changes frame.
                if (SAO_Prime_Cache_Node::Advance())
                    InvalidateRect(window, nullptr, FALSE);
                else
                {
                    KillTimer(window, ANIMATION);
                    animation_started = false;
                    MessageBoxW(window, L"Prepared Prime animation frame could not be presented.",
                        L"SAO Idle", MB_OK | MB_ICONERROR);
                }
            }
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT paint{};
            HDC destination = BeginPaint(window, &paint);
            const bool drawn = SAO_Prime_Cache_Node::Present(destination);
            EndPaint(window, &paint);
            if (drawn && !preload_scheduled)
            {
                preload_scheduled = true;
                if (!SetTimer(window, PRELOAD, 2000, nullptr))
                    MessageBoxW(window, L"The two-second preload timer could not start.",
                        L"SAO Idle", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        case WM_DESTROY:
            KillTimer(window, PRELOAD);
            KillTimer(window, ANIMATION);
            if (inventoryWorker) {
                WaitForSingleObject(inventoryWorker, INFINITE);
                CloseHandle(inventoryWorker); inventoryWorker = nullptr;
            }
            // Wait for upstream worker threads before destroying shared state.
            SAO_Alphabet_Startup::Stop();
            SAO_Alphabet_Conductor::Detach();
            SAO_Main_Screen_Math::Stop();
            SAO_Settlement_Math::Stop();
            SAO_Harvest_Tall_Grass_Math::Stop();
            SAO_Character_Harvest_Source::Stop();
            SAO_Prime_Cache_Node::Stop();
            if (SAO_Main_Screen_Conductor::Cache.bitmap)
                DeleteObject(SAO_Main_Screen_Conductor::Cache.bitmap);
            SAO_Main_Screen_Conductor::Cache.bitmap = nullptr;
            SAO_Main_Screen_Conductor::Cache.pixels = nullptr;
            projection = nullptr;
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    HWND Build(HINSTANCE instance, int showCommand)
    {
        show_mode = showCommand;
        SetProcessDPIAware();
        WNDCLASSW boundary{};
        boundary.lpfnWndProc = Receive;
        boundary.hInstance = instance;
        boundary.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        boundary.lpszClassName = L"SAO_Idle_Experiment";
        if (!RegisterClassW(&boundary)) return nullptr;
        HWND window = CreateWindowExW(0, boundary.lpszClassName, L"SAO Idle",
            WS_POPUP, 0, 0, WIDTH, HEIGHT, nullptr, nullptr, instance, nullptr);
        if (!window) return nullptr;
        // The program catches and displays ONE final projection as before.
        if (!SAO_Prime_Cache_Node::Start(window) ||
            !SAO_Main_Screen_Math::Start(window))
        {
            MessageBoxW(window, L"The source caches could not start.",
                L"SAO Idle", MB_OK | MB_ICONERROR);
            DestroyWindow(window);
            return nullptr;
        }
        return window;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    HWND window = SAO_Boundary::Build(instance, showCommand);
    if (!window) return 1;
    MSG message{};
    BOOL received;
    while ((received = GetMessageW(&message, nullptr, 0, 0)) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (received == -1) DestroyWindow(window);
    return received == -1 ? 1 : static_cast<int>(message.wParam);
}

