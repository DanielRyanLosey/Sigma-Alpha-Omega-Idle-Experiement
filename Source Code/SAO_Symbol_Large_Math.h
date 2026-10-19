#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Symbol_Large_Math
{
    constexpr std::size_t COUNT = 16;

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
        std::array<BitmapCacheNode, COUNT> symbols{};
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;
    const OutputNode& Build();
}
