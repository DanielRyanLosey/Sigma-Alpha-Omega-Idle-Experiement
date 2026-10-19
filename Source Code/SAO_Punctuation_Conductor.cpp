#include "SAO_Punctuation_Conductor.h"

namespace SAO_Punctuation_Conductor
{
    OutputNode Cache{};

    namespace
    {
        template<class Source>
        void Connect(const Source& source, std::array<CacheNode, 32>& target)
        {
            for (std::size_t i = 0; i < target.size(); ++i)
            {
                const auto& item = source.punctuation[i];
                target[i] = { &item.name, &item.path,
                    item.width, item.height, item.bytes, item.ready };
            }
        }

        void Group(std::size_t size, std::size_t group,
            std::size_t first, std::size_t last)
        {
            GroupNode& node = Cache.groups[size][group];
            node.count = static_cast<std::uint32_t>(last - first + 1);
            for (std::size_t i = first; i <= last; ++i)
                node.symbols[i - first] = &Cache.symbols[size][i];
        }
    }

    const OutputNode& Build(const SAO_Punctuation_Small_Math::OutputNode& small,
        const SAO_Punctuation_Medium_Math::OutputNode& medium,
        const SAO_Punctuation_Large_Math::OutputNode& large)
    {
        Cache = OutputNode{};
        if (!small.ready || !medium.ready || !large.ready)
        {
            Cache.error = !small.ready ? small.error :
                !medium.ready ? medium.error : large.error;
            if (Cache.error.empty()) Cache.error = L"A Punctuation Math cache is not ready.";
            return Cache;
        }

        Connect(small, Cache.symbols[0]);
        Connect(medium, Cache.symbols[1]);
        Connect(large, Cache.symbols[2]);

        for (const auto& family : Cache.symbols)
        {
            for (const auto& item : family)
            {
                if (!item.ready || !item.name || !item.path ||
                    item.name->empty() || item.path->empty() || !item.bytes)
                {
                    Cache.error = L"A Punctuation bitmap cache is incomplete.";
                    return Cache;
                }
            }
        }

        for (std::size_t size = 0; size < 3; ++size)
        {
            Group(size, 0, 0, 9);   // First 10 source PNGs.
            Group(size, 1, 10, 19); // Next 10 source PNGs.
            Group(size, 2, 20, 29); // Next 10 source PNGs.
            Group(size, 3, 30, 31); // Last 2 source PNGs.
        }
        Cache.ready = true;
        return Cache;
    }

    const CacheNode* Find(Size size, Symbol symbol)
    {
        if (!Cache.ready) return nullptr;
        const std::size_t family = static_cast<std::size_t>(size);
        const std::size_t index = static_cast<std::size_t>(symbol);
        if (family >= Cache.groups.size() || index >= 32) return nullptr;
        const std::size_t group = index / 10;
        const CacheNode* result = Cache.groups[family][group].symbols[index - group * 10];
        return result && result->ready ? result : nullptr;
    }

    const CacheNode* Find(Size size, wchar_t character)
    {
        switch (character)
        {
        case L'&': return Find(size, Symbol::Ampersand);
        case L'\'': return Find(size, Symbol::Apostrophe);
        case L'*': return Find(size, Symbol::Asterisk);
        case L'@': return Find(size, Symbol::At);
        case L'\\': return Find(size, Symbol::Backslash);
        case L'^': return Find(size, Symbol::Caret);
        case L':': return Find(size, Symbol::Colon);
        case L',': return Find(size, Symbol::Comma);
        case L'$': return Find(size, Symbol::Dollar);
        case L'"': return Find(size, Symbol::Double_Quote);
        case L'=': return Find(size, Symbol::Equals);
        case L'!': return Find(size, Symbol::Exclamation);
        case L'`': return Find(size, Symbol::Grave);
        case L'>': return Find(size, Symbol::Greater_Than);
        case L'#': return Find(size, Symbol::Hash);
        case L'-': return Find(size, Symbol::Hyphen);
        case L'{': return Find(size, Symbol::Left_Brace);
        case L'[': return Find(size, Symbol::Left_Bracket);
        case L'(': return Find(size, Symbol::Left_Parenthesis);
        case L'<': return Find(size, Symbol::Less_Than);
        case L'%': return Find(size, Symbol::Percentage);
        case L'.': return Find(size, Symbol::Period);
        case L'+': return Find(size, Symbol::Plus);
        case L'?': return Find(size, Symbol::Question);
        case L'}': return Find(size, Symbol::Right_Brace);
        case L']': return Find(size, Symbol::Right_Bracket);
        case L')': return Find(size, Symbol::Right_Parenthesis);
        case L';': return Find(size, Symbol::Semicolon);
        case L'/': return Find(size, Symbol::Slash);
        case L'~': return Find(size, Symbol::Tilde);
        case L'_': return Find(size, Symbol::Underscore);
        case L'|': return Find(size, Symbol::Vertical_Bar);
        default: return nullptr;
        }
    }

    RouteNode Route(Size size, Symbol left, Symbol right,
        Polarity polarity, Orientation orientation)
    {
        RouteNode result{};
        result.orientation = orientation;
        const CacheNode* first = Find(size, left);
        const CacheNode* second = Find(size, right);
        if (!first || !second ||
            (polarity != Polarity::Plus && polarity != Polarity::Minus))
            return result;

        if (orientation == Orientation::Left)
        {
            result.operations[0] = { first, Polarity::Plus };
            result.operations[1] = { second, polarity };
        }
        else if (orientation == Orientation::Right)
        {
            if (polarity == Polarity::Plus)
            {
                result.operations[0] = { second, Polarity::Plus };
                result.operations[1] = { first, Polarity::Plus };
            }
            else
            {
                result.operations[0] = { first, Polarity::Minus };
                result.operations[1] = { second, Polarity::Plus };
            }
        }
        else return result;
        result.ready = true;
        return result;
    }

    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation)
    {
        const CacheNode* first = Find(size, left);
        const CacheNode* second = Find(size, right);
        RouteNode result{};
        result.orientation = orientation;
        if (!first || !second ||
            (polarity != Polarity::Plus && polarity != Polarity::Minus))
            return result;

        if (orientation == Orientation::Left)
        {
            result.operations[0] = { first, Polarity::Plus };
            result.operations[1] = { second, polarity };
        }
        else if (orientation == Orientation::Right)
        {
            if (polarity == Polarity::Plus)
            {
                result.operations[0] = { second, Polarity::Plus };
                result.operations[1] = { first, Polarity::Plus };
            }
            else
            {
                result.operations[0] = { first, Polarity::Minus };
                result.operations[1] = { second, Polarity::Plus };
            }
        }
        else return result;
        result.ready = true;
        return result;
    }
}