#include "SAO_Numbers_Conductor.h"

namespace SAO_Numbers_Conductor
{
    OutputNode Cache{};

    namespace
    {
        template<class Source>
        void Connect(const Source& source, std::array<CacheNode, 10>& target)
        {
            for (std::size_t i = 0; i < target.size(); ++i)
            {
                const auto& number = source.numbers[i];
                target[i] = { &number.name, &number.path,
                    number.width, number.height, number.bytes, number.ready };
            }
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

    const OutputNode& Build(const SAO_Numbers_Small_Math::OutputNode& small,
        const SAO_Numbers_Medium_Math::OutputNode& medium,
        const SAO_Numbers_Large_Math::OutputNode& large)
    {
        Cache = OutputNode{};
        if (!small.ready || !medium.ready || !large.ready)
        {
            Cache.error = !small.ready ? small.error :
                !medium.ready ? medium.error : large.error;
            if (Cache.error.empty()) Cache.error = L"A Numbers Math cache is not ready.";
            return Cache;
        }

        Connect(small, Cache.numbers[0]);
        Connect(medium, Cache.numbers[1]);
        Connect(large, Cache.numbers[2]);

        for (const auto& family : Cache.numbers)
        {
            for (const auto& number : family)
            {
                if (!number.ready || !number.name || !number.path ||
                    number.name->empty() || number.path->empty() || !number.bytes)
                {
                    Cache.error = L"A Numbers bitmap cache is incomplete.";
                    return Cache;
                }
            }
        }

        for (std::size_t size = 0; size < 3; ++size)
        {
            GroupNode& group = Cache.groups[size];
            group.count = 10;
            for (std::size_t i = 0; i < 10; ++i)
                group.numbers[i] = &Cache.numbers[size][i];
        }
        Cache.ready = true;
        return Cache;
    }

    const CacheNode* Find(Size size, Number number)
    {
        if (!Cache.ready) return nullptr;
        const std::size_t family = static_cast<std::size_t>(size);
        const std::size_t index = static_cast<std::size_t>(number);
        if (family >= Cache.groups.size() || index >= 10) return nullptr;
        const CacheNode* result = Cache.groups[family].numbers[index];
        return result && result->ready ? result : nullptr;
    }

    const CacheNode* Find(Size size, wchar_t digit)
    {
        if (digit < L'0' || digit > L'9') return nullptr;
        return Find(size, static_cast<Number>(digit - L'0'));
    }

    RouteNode Route(Size size, Number left, Number right,
        Polarity polarity, Orientation orientation)
    {
        return SignedRoute(Find(size, left), Find(size, right), polarity, orientation);
    }

    RouteNode Route(Size size, wchar_t left, wchar_t right,
        Polarity polarity, Orientation orientation)
    {
        return SignedRoute(Find(size, left), Find(size, right), polarity, orientation);
    }
}