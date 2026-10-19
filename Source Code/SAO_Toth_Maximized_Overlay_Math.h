#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace SAO_Toth_Maximized_Overlay_Math
{
    constexpr std::size_t COUNT = 67;

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
        std::array<BitmapCacheNode, COUNT> assets{};
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;
    const OutputNode& Build();
}

