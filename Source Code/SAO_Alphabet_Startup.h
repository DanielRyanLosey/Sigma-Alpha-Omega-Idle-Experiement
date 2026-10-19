#pragma once

// One source-side launch after Program's existing two-second preload signal.
// It builds each upstream cache in order, then forwards to Alphabet -> Toth.
// Toth NEVER calls anything in this source stage.
namespace SAO_Alphabet_Startup
{
    bool Start();
    void Stop();
}
