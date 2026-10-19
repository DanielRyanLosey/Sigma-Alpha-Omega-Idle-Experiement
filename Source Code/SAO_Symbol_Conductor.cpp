#include "SAO_Symbol_Conductor.h"

namespace SAO_Symbol_Conductor
{
    OutputNode Cache{};

    namespace
    {
        template<class Source>
        void Connect(const Source& source, std::array<CacheNode, 16>& target)
        {
            for (std::size_t i = 0; i < target.size(); ++i)
            {
                const auto& symbol = source.symbols[i];
                target[i] = { &symbol.name, &symbol.path,
                    symbol.width, symbol.height, symbol.bytes, symbol.ready };
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

        RouteNode SignedRoute(const CacheNode* first, const CacheNode* second,
            Polarity polarity, Orientation orientation)
        {
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

    const OutputNode& Build(const SAO_Symbol_Small_Math::OutputNode& small,
        const SAO_Symbol_Medium_Math::OutputNode& medium,
        const SAO_Symbol_Large_Math::OutputNode& large)
    {
        Cache = OutputNode{};
        if (!small.ready || !medium.ready || !large.ready)
        {
            Cache.error = !small.ready ? small.error :
                !medium.ready ? medium.error : large.error;
            if (Cache.error.empty()) Cache.error = L"A Symbol Math cache is not ready.";
            return Cache;
        }

        Connect(small, Cache.symbols[0]);
        Connect(medium, Cache.symbols[1]);
        Connect(large, Cache.symbols[2]);

        for (const auto& family : Cache.symbols)
        {
            for (const auto& symbol : family)
            {
                if (!symbol.ready || !symbol.name || !symbol.path ||
                    symbol.name->empty() || symbol.path->empty() || !symbol.bytes)
                {
                    Cache.error = L"A Symbol bitmap cache is incomplete.";
                    return Cache;
                }
            }
        }

        for (std::size_t size = 0; size < 3; ++size)
        {
            Group(size, 0, 0, 9);    // First 10 source PNGs.
            Group(size, 1, 10, 15);  // Last 6 source PNGs.
        }
        Cache.ready = true;
        return Cache;
    }

    const CacheNode* Find(Size size, Symbol symbol)
    {
        if (!Cache.ready) return nullptr;
        const std::size_t family = static_cast<std::size_t>(size);
        const std::size_t index = static_cast<std::size_t>(symbol);
        if (family >= Cache.groups.size() || index >= 16) return nullptr;
        const std::size_t group = index / 10;
        const CacheNode* result = Cache.groups[family][group].symbols[index - group * 10];
        return result && result->ready ? result : nullptr;
    }

    const CacheNode* Find(Size size, wchar_t character)
    {
        switch (character)
        {
        case L'\u03B1': return Find(size, Symbol::Alpha_Lower);  // alpha
        case L'\u00B0': return Find(size, Symbol::Degree);
        case L'\u0394': return Find(size, Symbol::Delta);
        case L'\u00F7': return Find(size, Symbol::Divide);
        case L'\u2265': return Find(size, Symbol::Greater_Equal);
        case L'\u221E': return Find(size, Symbol::Infinity);
        case L'\u2264': return Find(size, Symbol::Less_Equal);
        case L'\u00D7': return Find(size, Symbol::Multiply);
        case L'\u2260': return Find(size, Symbol::Not_Equal);
        case L'\u03C9': return Find(size, Symbol::Omega_Lower);
        case L'\u03A9': return Find(size, Symbol::Omega);
        case L'\u03C0': return Find(size, Symbol::Pi);
        case L'\u00B1': return Find(size, Symbol::Plus_Minus);
        case L'\u03A3': return Find(size, Symbol::Sigma);
        case L'\u221A': return Find(size, Symbol::Square_Root);
        case L'\u03B8': return Find(size, Symbol::Theta);
        default: return nullptr;
        }
    }

    RouteNode Route(Size size, Symbol left, Symbol right,
        Polarity polarity, Orientation orientation)
    {
        return SignedRoute(Find(size, left), Find(size, right),
            polarity, orientation);
    }

    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation)
    {
        return SignedRoute(Find(size, left), Find(size, right),
            polarity, orientation);
    }
}