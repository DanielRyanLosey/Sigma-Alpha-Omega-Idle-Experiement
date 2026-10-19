#pragma once
#include "SAO_Upper_Conductor.h"
#include "SAO_Lower_Conductor.h"
#include "SAO_Symbol_Conductor.h"
#include "SAO_Punctuation_Conductor.h"
#include "SAO_Numbers_Conductor.h"
#include <cstdint>
#include <string>

namespace SAO_UP_LOW_SYM_PUN_NUM_Conductor
{
    enum class Size : std::uint8_t { Small, Medium, Large };
    enum class Orientation : std::uint8_t { Left, Right };

    // Owned by THIS conductor. Never points back to an upstream .cache file.
    // Pixel format: raw 32bpp premultiplied BGRA, top-down.
    struct BitmapCacheNode
    {
        std::wstring name;
        std::wstring path;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint64_t bytes = 0;
        Orientation orientation = Orientation::Left;
        bool ready = false;
    };

    struct OutputNode
    {
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;

    // Five completed conductor caches arrive FORWARD. No Math or PNG calls.
    const OutputNode& Build(
        const SAO_Upper_Conductor::OutputNode& upper,
        const SAO_Lower_Conductor::OutputNode& lower,
        const SAO_Symbol_Conductor::OutputNode& symbols,
        const SAO_Punctuation_Conductor::OutputNode& punctuation,
        const SAO_Numbers_Conductor::OutputNode& numbers);

    // Materialize ONE glyph inside this conductor's OWN bitmap cache.
    BitmapCacheNode Character(Size size, wchar_t character);

    // Join any sequence of existing glyph bitmaps at their native dimensions.
    // Left = source order; Right = reverse source order (glyphs NOT mirrored).
    // The resulting bitmap is written to THIS conductor's .cache before return.
    BitmapCacheNode Orient(Size size, const std::wstring& characters,
        Orientation orientation);

    // Two-character orientation entry point using the same recached output.
    BitmapCacheNode Orient(Size size, wchar_t left, wchar_t right,
        Orientation orientation);
}
