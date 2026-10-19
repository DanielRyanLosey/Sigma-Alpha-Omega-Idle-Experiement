#pragma once
#include "SAO_Numbers_Small_Math.h"
#include "SAO_Numbers_Medium_Math.h"
#include "SAO_Numbers_Large_Math.h"
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Numbers_Conductor
{
    enum class Size : std::uint8_t { Small, Medium, Large };
    enum class Orientation : std::uint8_t { Left, Right };
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };

    // Exact ten digit-cache positions, in 0-9 source order.
    enum class Number : std::uint8_t
    {
        Zero = 0, One = 1, Two = 2, Three = 3, Four = 4,
        Five = 5, Six = 6, Seven = 7, Eight = 8, Nine = 9
    };

    // Non-owning references to source Math bitmap caches.
    struct CacheNode
    {
        const std::wstring* name = nullptr;
        const std::wstring* path = nullptr;
        std::uint32_t width = 0, height = 0, bytes = 0;
        bool ready = false;
    };

    struct GroupNode
    {
        std::array<const CacheNode*, 10> numbers{};
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
        std::array<std::array<CacheNode, 10>, 3> numbers{}; // 3 sizes x 10 digits.
        std::array<GroupNode, 3> groups{};                   // One group 0-9 per size.
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;

    // Receives all three Math outputs; never invokes any Math Build().
    const OutputNode& Build(const SAO_Numbers_Small_Math::OutputNode& small,
        const SAO_Numbers_Medium_Math::OutputNode& medium,
        const SAO_Numbers_Large_Math::OutputNode& large);

    const CacheNode* Find(Size size, Number number);
    const CacheNode* Find(Size size, wchar_t digit);

    RouteNode Route(Size size, Number left, Number right,
        Polarity polarity, Orientation orientation);
    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation);
}
