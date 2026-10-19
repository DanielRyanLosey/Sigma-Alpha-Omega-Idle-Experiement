#pragma once
#include "SAO_Symbol_Small_Math.h"
#include "SAO_Symbol_Medium_Math.h"
#include "SAO_Symbol_Large_Math.h"
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Symbol_Conductor
{
    enum class Size : std::uint8_t { Small, Medium, Large };
    enum class Orientation : std::uint8_t { Left, Right };
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };

    // One symbol for each source PNG, exactly in screenshot order.
    enum class Symbol : std::uint8_t
    {
        Alpha_Lower = 0,
        Degree = 1,
        Delta = 2,
        Divide = 3,
        Greater_Equal = 4,
        Infinity = 5,
        Less_Equal = 6,
        Multiply = 7,
        Not_Equal = 8,
        Omega_Lower = 9,
        Omega = 10,
        Pi = 11,
        Plus_Minus = 12,
        Sigma = 13,
        Square_Root = 14,
        Theta = 15
    };

    // References the existing Math cache; does not create or decode any bitmap.
    struct CacheNode
    {
        const std::wstring* name = nullptr;
        const std::wstring* path = nullptr;
        std::uint32_t width = 0, height = 0, bytes = 0;
        bool ready = false;
    };

    struct GroupNode
    {
        std::array<const CacheNode*, 10> symbols{};
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
        std::array<std::array<CacheNode, 16>, 3> symbols{}; // 3 sizes x 16 symbols.
        std::array<std::array<GroupNode, 2>, 3> groups{}; // First 10, last 6.
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;

    // Three Math outputs -> cache references/grouping -> future conductor.
    // No Math Build(), file loading, bitmap copy, or rendering here.
    const OutputNode& Build(const SAO_Symbol_Small_Math::OutputNode& small,
        const SAO_Symbol_Medium_Math::OutputNode& medium,
        const SAO_Symbol_Large_Math::OutputNode& large);

    const CacheNode* Find(Size size, Symbol symbol);
    const CacheNode* Find(Size size, wchar_t character);

    RouteNode Route(Size size, Symbol left, Symbol right,
        Polarity polarity, Orientation orientation);
    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation);
}
