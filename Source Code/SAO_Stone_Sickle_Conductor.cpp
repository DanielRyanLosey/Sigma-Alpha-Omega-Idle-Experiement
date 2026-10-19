#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_Stone_Sickle_Conductor.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
namespace SAO_Stone_Sickle_Conductor {
    OutputNode Cache{};
    namespace {
        bool Group(const SAO_Static_PNG_Cache::BitmapNode& input, BitmapGroup& output) {
            if (!input.ready || !input.width || !input.height)return false;
            std::ifstream file(std::filesystem::path(input.path), std::ios::binary);
            if (!file)return false;
            output = {}; output.bitmap = &input; output.width = input.width; output.height = input.height;
            output.left = int(input.width); output.top = int(input.height);
            std::vector<BYTE> row(std::size_t(input.width) * 4);
            for (UINT y = 0; y < input.height; ++y) {
                if (!file.read(reinterpret_cast<char*>(row.data()), row.size()))return false;
                for (UINT x = 0; x < input.width; ++x) {
                    const BYTE* p = row.data() + std::size_t(x) * 4;
                    if (!p[3])continue;
                    output.pixels.push_back({ x,y,{p[0],p[1],p[2],p[3]} });
                    output.left = (std::min)(output.left, int(x)); output.top = (std::min)(output.top, int(y));
                    output.right = (std::max)(output.right, int(x)); output.bottom = (std::max)(output.bottom, int(y));
                }
            }
            return file.peek() == EOF;
        }
    }
    const OutputNode& Build(const SAO_Stone_Sickle_Math::OutputNode& source) {
        if (Cache.ready)return Cache;
        if (!source.ready) { Cache.error = source.error; return Cache; }
        for (unsigned i = 0; i < Cache.steps.size(); ++i) {
            auto& step = Cache.steps[i]; step.next = (i + 1) % SAO_Stone_Sickle_Math::COUNT;
            // Source canvas stays native. Item padding never changes these poses.
            if (!Group(source.frames[i], step.group)) {
                Cache.error = L"Sickle frame grouping failed: " + source.frames[i].name; return Cache;
            }
        }
        if (!Group(source.item, Cache.item) || Cache.item.pixels.empty()) {
            Cache.error = L"Sickle item grouping failed or PNG is fully transparent."; return Cache;
        }
        Cache.error.clear(); Cache.ready = true; return Cache;
    }
}