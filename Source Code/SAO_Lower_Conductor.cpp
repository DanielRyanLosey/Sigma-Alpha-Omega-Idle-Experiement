#include "SAO_Lower_Conductor.h"

namespace SAO_Lower_Conductor
{
    OutputNode Cache{};

    namespace
    {
        template<class Source>
        void Connect(const Source& source, std::array<CacheNode, 26>& target)
        {
            for (std::size_t i = 0; i < target.size(); ++i)
            {
                const auto& letter = source.letters[i];
                target[i] = { &letter.name, &letter.path,
                    letter.width, letter.height, letter.bytes, letter.ready };
            }
        }

        void Group(std::size_t size, std::size_t group,
            std::size_t first, std::size_t last)
        {
            GroupNode& node = Cache.groups[size][group];
            node.first = static_cast<wchar_t>(L'a' + first);
            node.last = static_cast<wchar_t>(L'a' + last);
            node.count = static_cast<std::uint32_t>(last - first + 1);
            for (std::size_t i = first; i <= last; ++i)
                node.letters[i - first] = &Cache.letters[size][i];
        }

        bool AllReady(const SAO_Lower_Small_Math::OutputNode& small,
            const SAO_Lower_Medium_Math::OutputNode& medium,
            const SAO_Lower_Large_Math::OutputNode& large)
        {
            if (!small.ready || !medium.ready || !large.ready) return false;
            for (const auto& family : Cache.letters)
                for (const auto& letter : family)
                    if (!letter.ready || !letter.name || !letter.path ||
                        letter.name->empty() || letter.path->empty() || !letter.bytes)
                        return false;
            return true;
        }
    }

    const OutputNode& Build(const SAO_Lower_Small_Math::OutputNode& small,
        const SAO_Lower_Medium_Math::OutputNode& medium,
        const SAO_Lower_Large_Math::OutputNode& large)
    {
        Cache = OutputNode{};
        if (!small.ready || !medium.ready || !large.ready)
        {
            Cache.error = !small.ready ? small.error :
                !medium.ready ? medium.error : large.error;
            if (Cache.error.empty()) Cache.error = L"A Lower Math cache is not ready.";
            return Cache;
        }

        Connect(small, Cache.letters[0]);
        Connect(medium, Cache.letters[1]);
        Connect(large, Cache.letters[2]);

        if (!AllReady(small, medium, large))
        {
            Cache.error = L"A Lower PNG bitmap cache is missing or incomplete.";
            return Cache;
        }

        for (std::size_t size = 0; size < 3; ++size)
        {
            Group(size, 0, 0, 9);   // a-j
            Group(size, 1, 10, 19); // k-t
            Group(size, 2, 20, 25); // u-z
        }
        Cache.ready = true;
        return Cache;
    }

    const CacheNode* Find(Size size, wchar_t letter)
    {
        if (!Cache.ready || letter < L'a' || letter > L'z') return nullptr;
        const std::size_t family = static_cast<std::size_t>(size);
        if (family >= Cache.groups.size()) return nullptr;
        const std::size_t index = static_cast<std::size_t>(letter - L'a');
        const std::size_t group = index < 10 ? 0 : index < 20 ? 1 : 2;
        const GroupNode& node = Cache.groups[family][group];
        const CacheNode* result = node.letters[index - (group == 0 ? 0 : group == 1 ? 10 : 20)];
        return result && result->ready ? result : nullptr;
    }

    RouteNode Route(Size size, wchar_t left, wchar_t right,
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
}