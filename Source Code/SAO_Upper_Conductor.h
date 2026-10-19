#pragma once
#include "SAO_Upper_Small_Math.h"
#include "SAO_Upper_Medium_Math.h"
#include "SAO_Upper_Large_Math.h"
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Upper_Conductor
{
    enum class Size : std::uint8_t { Small, Medium, Large };
    enum class Orientation : std::uint8_t { Left, Right };
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };

    // A reference to an existing Math cache, not a second bitmap or PNG decoder.
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
        std::array<std::array<GroupNode, 3>, 3> groups{}; // A-J, K-T, U-Z.
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;

    // Three Math outputs -> grouped references -> conductor output.
    // Never calls a Math Build(), decodes PNGs, or reaches into the Main Screen.
    const OutputNode& Build(const SAO_Upper_Small_Math::OutputNode& small,
        const SAO_Upper_Medium_Math::OutputNode& medium,
        const SAO_Upper_Large_Math::OutputNode& large);

    // Single-glyph lookup for Thoth and character input.
    const CacheNode* Find(Size size, wchar_t letter);

    // Signed, ordered references; the NEXT conductor decides how to apply them.
    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation);
}

