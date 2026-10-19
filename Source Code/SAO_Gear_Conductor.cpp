#include "SAO_Gear_Conductor.h"
namespace SAO_Gear_Conductor {
    OutputNode Cache{};
    const OutputNode& Build(const SAO_Gear_Math::OutputNode& source) {
        if (Cache.ready)return Cache;
        if (!source.ready) { Cache.error = source.error; return Cache; }
        Cache.group.window = &source.window;
        for (unsigned i = 0; i < SAO_Gear_Math::COUNT; ++i) {
            Cache.group.locations[i] = &source.locations[i];
            Cache.group.anchors[i] = SAO_Static_PNG_Cache::Locate(source.locations[i]);
            if (!Cache.group.anchors[i].ready) {
                Cache.error = L"Missing location mark: " + source.locations[i].name; return Cache;
            }
        }
        return (Cache.ready = true), Cache;
    }
}