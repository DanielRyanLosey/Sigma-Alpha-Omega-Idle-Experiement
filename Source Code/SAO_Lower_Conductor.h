#pragma once
#include "SAO_Lower_Small_Math.h"
#include "SAO_Lower_Medium_Math.h"
#include "SAO_Lower_Large_Math.h"
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Lower_Conductor
{
    enum class Size : std::uint8_t { Small, Medium, Large };
    enum class Orientation : std::uint8_t { Left, Right };
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };

    struct CacheNode
    {
        const std::wstring* name = nullptr;
        const std::wstring* path = nullptr;
        std::uint32_t width = 0, height = 0, bytes = 0;
        bool ready = false;
    };

    struct GroupNode
    {
        wchar_t first = L'\0', last = L'\0';
        std::array<const CacheNode*, 10> letters{};
        std::uint32_t count = 0;
    };

    struct OperationNode
    {
        const CacheNode* cache = nullptr;
        Polarity sign = Polarity::Plus;
    };

    struct RouteNode
    {
        std::array<OperationNode, 2> operations{};
        Orientation orientation = Orientation::Left;
        bool ready = false;
    };

    struct OutputNode
    {
        std::array<std::array<CacheNode, 26>, 3> letters{}; // Small, Medium, Large.
        std::array<std::array<GroupNode, 3>, 3> groups{};   // a-j, k-t, u-z.
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;

    const OutputNode& Build(const SAO_Lower_Small_Math::OutputNode& small,
        const SAO_Lower_Medium_Math::OutputNode& medium,
        const SAO_Lower_Large_Math::OutputNode& large);

    const CacheNode* Find(Size size, wchar_t letter);

    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation);
}
