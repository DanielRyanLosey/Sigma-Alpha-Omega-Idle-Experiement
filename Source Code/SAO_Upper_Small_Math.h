#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Upper_Small_Math
{
    constexpr std::size_t COUNT = 26;

    // This bitmap cache belongs only to the Small Math source.
    struct BitmapCacheNode
    {
        std::wstring name;
        std::wstring path;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint32_t bytes = 0;
        bool ready = false;
    };

    struct OutputNode
    {
        std::array<BitmapCacheNode, COUNT> letters{}; // A through Z, in order.
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;
    const OutputNode& Build(); // PNG -> premultiplied BGRA bitmap -> cache; nothing else.
}

