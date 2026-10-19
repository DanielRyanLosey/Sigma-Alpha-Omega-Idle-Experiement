#pragma once
#include "SAO_Punctuation_Small_Math.h"
#include "SAO_Punctuation_Medium_Math.h"
#include "SAO_Punctuation_Large_Math.h"
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Punctuation_Conductor
{
    enum class Size : std::uint8_t { Small, Medium, Large };
    enum class Orientation : std::uint8_t { Left, Right };
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };

    // Exactly one index for each punctuation bitmap, in the screenshot order.
    enum class Symbol : std::uint8_t
    {
        Ampersand = 0,
        Apostrophe = 1,
        Asterisk = 2,
        At = 3,
        Backslash = 4,
        Caret = 5,
        Colon = 6,
        Comma = 7,
        Dollar = 8,
        Double_Quote = 9,
        Equals = 10,
        Exclamation = 11,
        Grave = 12,
        Greater_Than = 13,
        Hash = 14,
        Hyphen = 15,
        Left_Brace = 16,
        Left_Bracket = 17,
        Left_Parenthesis = 18,
        Less_Than = 19,
        Percentage = 20,
        Period = 21,
        Plus = 22,
        Question = 23,
        Right_Brace = 24,
        Right_Bracket = 25,
        Right_Parenthesis = 26,
        Semicolon = 27,
        Slash = 28,
        Tilde = 29,
        Underscore = 30,
        Vertical_Bar = 31
    };

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
        std::array<std::array<CacheNode, 32>, 3> symbols{};
        std::array<std::array<GroupNode, 4>, 3> groups{};
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;

    // Existing Math outputs are consumed forward; no Math Build(), PNG decoding, or rendering here.
    const OutputNode& Build(const SAO_Punctuation_Small_Math::OutputNode& small,
        const SAO_Punctuation_Medium_Math::OutputNode& medium,
        const SAO_Punctuation_Large_Math::OutputNode& large);

    const CacheNode* Find(Size size, Symbol symbol);
    const CacheNode* Find(Size size, wchar_t character);

    RouteNode Route(Size size, Symbol left, Symbol right,
        Polarity polarity, Orientation orientation);
    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation);
}
