#pragma once
#include "SAO_Toth_Overlay_Conductor.h"
#include <cstdint>
#include <string>
#include <windows.h>
#include "SAO_Alphabet_Conductor.h"

namespace SAO_Toth
{
    // Toth owns its overlay state only. The program never receives it directly.
    struct TextLocation
    {
        int x = 0, y = 0, width = 0, height = 0;
        bool ready = false;
    };

    struct OutputNode
    {
        SAO_Toth_Overlay_Conductor::View view =
            SAO_Toth_Overlay_Conductor::View::Maximized;
        std::wstring overlayPath;
        std::wstring maximizedPath; // Both static composites forwarded after preload
        std::wstring minimizedPath;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint64_t revision = 0;
        bool ready = false;
        bool validated = false; // True only after the program's 2-second preload signal.
        // Changing content is independent of the cached overlay PNGs.
        std::wstring inputText;
        std::wstring tothText;
        std::wstring bastetText;
        TextLocation inputLocation, minimizedInputLocation;
        TextLocation tothOutputLocation, bastetOutputLocation;
        TextLocation playerNameLocation; // PNG-defined box, not a fixed rectangle.
        std::wstring playerName;         // Active persistent profile name.
        // Forwarded selected icon CACHE paths; independent of the static overlay.
        std::wstring selectedResearchIcon;
        std::wstring selectedPlayerIcon;
        bool awaitingPlayerName = false; // #Player Name -> #Full Name
        bool englishReady = false;
        std::uint64_t alphabetRevision = 0;
        std::uint64_t answerRevision = 0;
        bool exitRequested = false; // Forwarded to shared node, never handled by a back-call.
        bool backRequested = false; // Forwarded #Back event, consumed at Prime.
        bool startGameRequested = false;
        bool harvestTallGrassRequested = false;
        bool inventoryRequested = false;
        bool gearRequested = false;
        bool progressSelected = false;
        std::wstring error;
    };
    extern OutputNode Cache;

    // Called once by Prime Conductor to launch the independent Toth branch.
    // Toth -> SAO_Prime_Cache_Node, NEVER directly to Program.
    bool Start(HWND target);
    // Signal the existing one-time two-second preload event; no Toth animation timer.
    void Preload();
    // Minimize and minimized Enter restore. PNG-alpha click only.
    bool Click(std::uint32_t x, std::uint32_t y, HWND target);
    // Only user events change text; never called on an animation tick.
    // Only an already-produced Alphabet output is accepted.
    // No Build() or Process() calls back to preceding Maths or Conductors.
    bool ReceiveAlphabet(const SAO_Alphabet_Conductor::OutputNode& language);
    void Stop();
}






