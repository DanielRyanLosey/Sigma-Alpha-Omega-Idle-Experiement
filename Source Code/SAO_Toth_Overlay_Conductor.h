#pragma once
#include "SAO_Toth_Maximized_Overlay_Math.h"
#include "SAO_Toth_Minimized_Overlay_Math.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace SAO_Toth_Overlay_Conductor
{
    enum class View : std::uint8_t { Maximized, Minimized };
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };
    enum class Group : std::uint8_t
    {
        Frame, Navigation, Dialogue, Input, Learning, Research, Player
    };
    enum class Role : std::uint8_t
    {
        Frame, Button, Location, Output, Icon, Lock
    };

    // Describes functionality; bitmap data is not passed on as upstream pointers.
    struct LayerNode
    {
        std::wstring name;
        Group group = Group::Frame;
        Role role = Role::Frame;
        bool included = false;      // (+) present, (-) absent
        bool clickable = false;     // Only an actual Button PNG
    };

    struct BitmapCacheNode
    {
        std::wstring path;         // Conductor-owned COMPOSED PBGRA cache
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint64_t bytes = 0;
        bool ready = false;
        std::vector<LayerNode> layers;
    };

    struct OutputNode
    {
        BitmapCacheNode maximized;
        BitmapCacheNode minimized;
        std::wstring error;
        bool ready = false;
    };

    struct ClickNode
    {
        std::wstring button;       // Exact source asset name for the next conductor
        View view = View::Maximized;
        bool ready = false;
    };

    extern OutputNode Cache;

    // Receives the existing bitmap CACHE outputs. Does NOT call either Math Build().
    // Produces only the requested view at first; the other view is on demand.
    const OutputNode& Build(
        const SAO_Toth_Maximized_Overlay_Math::OutputNode& maximized,
        const SAO_Toth_Minimized_Overlay_Math::OutputNode& minimized,
        View initialView = View::Maximized);

    // Layer composition order is fixed and forward-only.
    // Minimized: +(Input_Location + Input_Frame) + (Input_Enter_Button).
    const BitmapCacheNode& Compose(View view);

    // Add/remove an individual layer (e.g. Lock_Icon or Rank_2_Icon), then re-cache.
    bool Apply(View view, std::wstring_view assetName, Polarity operation);

    // Alpha test of the cached Button PNG ONLY; no rectangle hitboxes.
    // OnClick decisions are returned for Toth/next conductor to handle.
    ClickNode Click(View view, std::uint32_t x, std::uint32_t y);
}



