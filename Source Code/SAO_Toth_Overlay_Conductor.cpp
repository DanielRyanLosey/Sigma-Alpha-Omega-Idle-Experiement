#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_Toth_Overlay_Conductor.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace SAO_Toth_Overlay_Conductor
{
    OutputNode Cache{};

    namespace
    {
        namespace fs = std::filesystem;
        struct LayerSpec
        {
            std::size_t index;
            Group group;
            Role role;
            bool initiallyIncluded;
        };

        // Fixed, deliberate layer order. All 67 assets occur exactly once.
        // Location/output layers stay independent of future changing text.
        // Lock/rank/stage icons stay independent for explicit +/- operations.
        constexpr LayerSpec MAX_LAYERS[] =
        {
        { 24, Group::Frame, Role::Frame, true }, // SAO_Toth_Overlay_Frame
        { 0, Group::Dialogue, Role::Output, true }, // SAO_Toth_Baetet_Output_Location
        { 64, Group::Dialogue, Role::Output, true }, // SAO_Toth_Toth_Output_Location
        { 31, Group::Input, Role::Location, true }, // SAO_Toth_Player_Input_Location
        { 1, Group::Learning, Role::Location, true }, // SAO_Toth_Learn_1_Level_Location
        { 3, Group::Learning, Role::Location, true }, // SAO_Toth_Learn_2_Level_Location
        { 5, Group::Learning, Role::Location, true }, // SAO_Toth_Learn_3_Level_Location
        { 7, Group::Learning, Role::Location, true }, // SAO_Toth_Learn_4_Level_Location
        { 9, Group::Learning, Role::Location, true }, // SAO_Toth_Learn_5_Level_Location
        { 11, Group::Learning, Role::Location, true }, // SAO_Toth_Learn_6_Level_Location
        { 46, Group::Research, Role::Location, true }, // SAO_Toth_Research_Age_Location
        { 54, Group::Research, Role::Location, true }, // SAO_Toth_Research_Percentage_Location
        { 34, Group::Player, Role::Location, true }, // SAO_Toth_Player_Level_Completion_Location
        { 35, Group::Player, Role::Location, true }, // SAO_Toth_Player_Level_Percent_Location
        { 36, Group::Player, Role::Location, true }, // SAO_Toth_Player_Name_Location
        { 61, Group::Navigation, Role::Button, true }, // SAO_Toth_Toth_Button
        { 14, Group::Navigation, Role::Button, true }, // SAO_Toth_Learn_Button
        { 47, Group::Navigation, Role::Button, true }, // SAO_Toth_Research_Button
        { 45, Group::Navigation, Role::Button, true }, // SAO_Toth_Progress_Button
        { 59, Group::Navigation, Role::Button, true }, // SAO_Toth_System_Button
        { 23, Group::Navigation, Role::Button, true }, // SAO_Toth_Minimize_Button
        { 60, Group::Dialogue, Role::Button, true }, // SAO_Toth_Toth_Break_Down_Button
        { 62, Group::Dialogue, Role::Button, true }, // SAO_Toth_Toth_Example_Button
        { 63, Group::Dialogue, Role::Button, true }, // SAO_Toth_Toth_Explain_Button
        { 65, Group::Dialogue, Role::Button, true }, // SAO_Toth_Toth_Practice_Button
        { 66, Group::Dialogue, Role::Button, true }, // SAO_Toth_Toth_Quiz_Button
        { 30, Group::Input, Role::Button, true }, // SAO_Toth_Player_Input_Enter_Button
        { 15, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Learn_1_Button
        { 16, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Learn_2_Button
        { 17, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Learn_3_Button
        { 18, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Learn_4_Button
        { 19, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Learn_5_Button
        { 20, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Learn_6_Button
        { 21, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Stage_1_Button
        { 22, Group::Learning, Role::Button, true }, // SAO_Toth_Learn_Stage_2_Button
        { 25, Group::Player, Role::Button, true }, // SAO_Toth_Player_Application_Button
        { 27, Group::Player, Role::Button, true }, // SAO_Toth_Player_Discipline_Button
        { 32, Group::Player, Role::Button, true }, // SAO_Toth_Player_Intelligence_Button
        { 37, Group::Player, Role::Button, true }, // SAO_Toth_Player_Physique_Button
        { 39, Group::Player, Role::Button, true }, // SAO_Toth_Player_Practice_Button
        { 43, Group::Player, Role::Button, true }, // SAO_Toth_Player_Wisdom_Button
        { 48, Group::Research, Role::Button, true }, // SAO_Toth_Research_History_Button
        { 50, Group::Research, Role::Button, true }, // SAO_Toth_Research_Language_Button
        { 52, Group::Research, Role::Button, true }, // SAO_Toth_Research_Mathematics_Button
        { 55, Group::Research, Role::Button, true }, // SAO_Toth_Research_Technology_Button
        { 57, Group::Research, Role::Button, true }, // SAO_Toth_Research_World_Lore_Button
        { 49, Group::Research, Role::Icon, true }, // SAO_Toth_Research_History_Icon
        { 51, Group::Research, Role::Icon, true }, // SAO_Toth_Research_Language_Icon
        { 53, Group::Research, Role::Icon, true }, // SAO_Toth_Research_Mathematics_Icon
        { 56, Group::Research, Role::Icon, true }, // SAO_Toth_Research_Technology_Icon
        { 58, Group::Research, Role::Icon, true }, // SAO_Toth_Research_World_Lore_Icon
        { 26, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Application_Icon
        { 28, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Discipline_Icon
        { 29, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Icon_LVL_Icon
        { 33, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Intelligence_Icon
        { 38, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Physique_Icon
        { 40, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Practice_Icon
        { 41, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Rank_1_Icon
        { 42, Group::Player, Role::Icon, false }, // SAO_Toth_Player_Rank_2_Icon
        { 44, Group::Player, Role::Icon, true }, // SAO_Toth_Player_Wisdom_Icon
        { 2, Group::Learning, Role::Lock, false }, // SAO_Toth_Learn_1_Lock_Icon
        { 4, Group::Learning, Role::Lock, true }, // SAO_Toth_Learn_2_Lock_Icon
        { 6, Group::Learning, Role::Lock, true }, // SAO_Toth_Learn_3_Lock_Icon
        { 8, Group::Learning, Role::Lock, true }, // SAO_Toth_Learn_4_Lock_Icon
        { 10, Group::Learning, Role::Lock, true }, // SAO_Toth_Learn_5_Lock_Icon
        { 12, Group::Learning, Role::Lock, true }, // SAO_Toth_Learn_6_Lock_Icon
        { 13, Group::Learning, Role::Icon, false }, // SAO_Toth_Learn_6_X2_Icon
        };

        // Explicit minimized grouping:
        // +(Input_Location + Input_Frame) + (Input_Enter_Button)
        constexpr LayerSpec MIN_LAYERS[] =
        {
            { 2, Group::Input, Role::Location, true }, // SAO_Toth_Minimized_Input_Location
            { 1, Group::Frame, Role::Frame, true },    // SAO_Toth_Minimized_Input_Frame
            { 0, Group::Input, Role::Button, true }   // SAO_Toth_Minimized_Input_Enter_Button
        };

        struct SourceNode
        {
            fs::path path; // PRIVATE: only used inside this conductor
            std::uint32_t width = 0;
            std::uint32_t height = 0;
            std::uint32_t bytes = 0;
        };

        std::vector<SourceNode> maxSources;
        std::vector<SourceNode> minSources;
        fs::path ownFolder;

        BitmapCacheNode& Target(View view)
        {
            return view == View::Minimized ? Cache.minimized : Cache.maximized;
        }
        const std::vector<SourceNode>& Sources(View view)
        {
            return view == View::Minimized ? minSources : maxSources;
        }

        bool Directory()
        {
            fs::path base;
#ifdef _WIN32
            wchar_t name[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, name, MAX_PATH);
            if (!n || n >= MAX_PATH)
            {
                Cache.error = L"Cannot locate the executable directory.";
                return false;
            }
            base = fs::path(std::wstring(name, n)).parent_path();
#else
            base = fs::current_path();
#endif
            ownFolder = base / L"Cache" / L"SAO_Toth_Overlay_Conductor";
            std::error_code ec;
            fs::create_directories(ownFolder, ec);
            if (ec)
            {
                Cache.error = L"Cannot create Toth conductor cache folder.";
                return false;
            }
            return true;
        }

        bool Check(std::uint32_t w, std::uint32_t h, std::uint32_t bytes)
        {
            return w && h && (std::uint64_t(w) * h * 4ULL == bytes);
        }

        template <typename MathOutput>
        bool Import(MathOutput const& input, View view,
            const LayerSpec* specs, std::size_t count)
        {
            BitmapCacheNode& target = Target(view);
            auto& dest = (view == View::Minimized ? minSources : maxSources);
            target.layers.clear();
            dest.clear();
            dest.reserve(count);
            target.layers.reserve(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                const auto& spec = specs[i];
                if (spec.index >= input.assets.size())
                {
                    Cache.error = L"Asset grouping index exceeds the Math output.";
                    return false;
                }
                const auto& png = input.assets[spec.index];
                if (!png.ready || png.name.empty() || png.path.empty() ||
                    !Check(png.width, png.height, png.bytes))
                {
                    Cache.error = L"Bitmap cache is missing or incomplete: " + png.name;
                    return false;
                }
                if (!target.width)
                {
                    target.width = png.width;
                    target.height = png.height;
                }
                else if (target.width != png.width || target.height != png.height)
                {
                    Cache.error = L"The source PNG canvases have different sizes: " + png.name;
                    return false; // assets are pre-positioned; no scaling/offsets
                }
                dest.push_back({ fs::path(png.path), png.width, png.height, png.bytes });
                target.layers.push_back({ png.name, spec.group, spec.role,
                    spec.initiallyIncluded, spec.role == Role::Button });
            }
            return true;
        }

        // Read a raw 32bpp WIC PBGRA cache into one reused scratch buffer.
        bool Read(const SourceNode& src, std::vector<std::uint8_t>& pixels)
        {
            std::ifstream in{ src.path, std::ios::binary };
            if (!in) return false;
            pixels.resize(src.bytes);
            in.read(reinterpret_cast<char*>(pixels.data()), src.bytes);
            return in.gcount() == static_cast<std::streamsize>(src.bytes);
        }

        // PBGRA premultiplied alpha, src OVER dst. No mirroring, resizing or offsets.
        void Overlay(std::vector<std::uint8_t>& dst,
            const std::vector<std::uint8_t>& src)
        {
            for (std::size_t i = 0; i < dst.size(); i += 4)
            {
                const std::uint32_t alpha = src[i + 3];
                if (!alpha) continue;
                if (alpha == 255)
                {
                    dst[i] = src[i];
                    dst[i + 1] = src[i + 1];
                    dst[i + 2] = src[i + 2];
                    dst[i + 3] = 255;
                    continue;
                }
                const std::uint32_t inverse = 255 - alpha;
                for (std::size_t channel = 0; channel < 4; ++channel)
                {
                    const std::uint32_t value = src[i + channel] +
                        (std::uint32_t(dst[i + channel]) * inverse + 127) / 255;
                    dst[i + channel] = static_cast<std::uint8_t>((std::min)(value, 255u));
                }
            }
        }

        // A cache belongs to its grouping state and its current source caches.
        // This compact sidecar is written once and reused; no bitmap is read on
        // a warm launch. The version invalidates caches from the earlier code.
        std::string StateText(View view)
        {
            const BitmapCacheNode& target = Target(view);
            const auto& sources = Sources(view);
            std::string result = "SAO_Toth_Overlay_Conductor:v2\n";
            result += (view == View::Maximized ? "maximized\n" : "minimized\n");
            for (std::size_t i = 0; i < target.layers.size(); ++i)
            {
                result += std::to_string(i);
                result += target.layers[i].included ? ":+\n" : ":-\n";
                result += fs::path(target.layers[i].name).u8string();
                result += "\n";
                result += sources[i].path.u8string();
                result += "\n";
            }
            return result;
        }

        bool ProjectionFresh(View view)
        {
            const BitmapCacheNode& target = Target(view);
            const auto& sources = Sources(view);
            if (sources.size() != target.layers.size()) return false;
            const fs::path path(target.path);
            const fs::path state(path.wstring() + L".state");
            std::error_code ec;
            const auto length = fs::file_size(path, ec);
            if (ec || length != std::uint64_t(target.width) * target.height * 4) return false;
            const auto composedStamp = fs::last_write_time(path, ec);
            if (ec) return false;
            std::ifstream saved(state, std::ios::binary);
            if (!saved) return false;
            const std::string expected = StateText(view);
            const std::string stored((std::istreambuf_iterator<char>(saved)),
                std::istreambuf_iterator<char>());
            if (stored != expected) return false;
            for (std::size_t i = 0; i < sources.size(); ++i)
            {
                if (!target.layers[i].included) continue;
                if (fs::file_size(sources[i].path, ec) != sources[i].bytes || ec)
                    return false;
                if (fs::last_write_time(sources[i].path, ec) > composedStamp || ec)
                    return false;
            }
            return true;
        }

        bool WriteState(View view)
        {
            const BitmapCacheNode& node = Target(view);
            const std::string content = StateText(view);
            const fs::path target(node.path + L".state");
            const fs::path temporary(node.path + L".state.tmp");
            {
                std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                if (!out) return false;
                out.write(content.data(), static_cast<std::streamsize>(content.size()));
                if (!out) return false;
            }
#ifdef _WIN32
            if (MoveFileExW(temporary.c_str(), target.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temporary.c_str());
            return false;
#else
            std::error_code ec;
            fs::rename(temporary, target, ec);
            return !ec;
#endif
        }

        bool Save(const fs::path& path, const std::vector<std::uint8_t>& data)
        {
            const fs::path temp = fs::path(path.wstring() + L".tmp");
            {
                std::ofstream out{ temp, std::ios::binary | std::ios::trunc };
                if (!out) return false;
                out.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<std::streamsize>(data.size()));
                if (!out) return false;
            }
#ifdef _WIN32
            if (!MoveFileExW(temp.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                DeleteFileW(temp.c_str());
                return false;
            }
#else
            std::error_code ec;
            fs::rename(temp, path, ec);
            if (ec) return false;
#endif
            return true;
        }
    }

    const OutputNode& Build(
        const SAO_Toth_Maximized_Overlay_Math::OutputNode& maximized,
        const SAO_Toth_Minimized_Overlay_Math::OutputNode& minimized,
        View initialView)
    {
        Cache = OutputNode{};
        maxSources.clear();
        minSources.clear();
        if (!maximized.ready || !minimized.ready)
        {
            Cache.error = !maximized.ready ? maximized.error : minimized.error;
            if (Cache.error.empty()) Cache.error = L"Toth source Math cache is not ready.";
            return Cache;
        }
        if (!Directory()) return Cache;
        if (!Import(maximized, View::Maximized, MAX_LAYERS,
            sizeof(MAX_LAYERS) / sizeof(MAX_LAYERS[0]))) return Cache;
        if (!Import(minimized, View::Minimized, MIN_LAYERS,
            sizeof(MIN_LAYERS) / sizeof(MIN_LAYERS[0]))) return Cache;

        Cache.maximized.path = (ownFolder / L"SAO_Toth_Maximized_Overlay.cache").wstring();
        Cache.minimized.path = (ownFolder / L"SAO_Toth_Minimized_Overlay.cache").wstring();
        Cache.ready = true;
        if (!Compose(initialView).ready)
            Cache.ready = false;
        return Cache;
    }

    const BitmapCacheNode& Compose(View view)
    {
        BitmapCacheNode& target = Target(view);
        target.ready = false;
        if (!Cache.ready || target.path.empty()) return target;
        // Cached projection is still exact for these sources and +/- layers.
        if (ProjectionFresh(view))
        {
            target.bytes = std::uint64_t(target.width) * target.height * 4;
            target.ready = true;
            return target;
        }
        const auto& sources = Sources(view);
        if (sources.size() != target.layers.size())
        {
            Cache.error = L"Overlay source/layer count mismatch.";
            return target;
        }
        const std::uint64_t total = std::uint64_t(target.width) * target.height * 4;
        if (total > (std::numeric_limits<std::size_t>::max)())
        {
            Cache.error = L"Overlay canvas exceeds process memory limits.";
            return target;
        }
        std::vector<std::uint8_t> composed(static_cast<std::size_t>(total), 0);
        std::vector<std::uint8_t> scratch;
        for (std::size_t i = 0; i < sources.size(); ++i)
        {
            if (!target.layers[i].included) continue;
            if (!Read(sources[i], scratch))
            {
                Cache.error = L"Cannot read bitmap cache: " + target.layers[i].name;
                return target;
            }
            Overlay(composed, scratch);
        }
        if (!Save(fs::path(target.path), composed))
        {
            Cache.error = L"Cannot write Toth overlay cache: " + target.path;
            return target;
        }
        if (!WriteState(view))
        {
            Cache.error = L"Cannot persist the Toth overlay grouping state.";
            return target;
        }
        target.bytes = total;
        target.ready = true;
        Cache.error.clear();
        return target;
    }

    bool Apply(View view, std::wstring_view name, Polarity operation)
    {
        if (!Cache.ready) return false;
        BitmapCacheNode& target = Target(view);
        for (auto& layer : target.layers)
        {
            if (layer.name != name) continue;
            const bool visible = operation == Polarity::Plus;
            if (layer.included == visible) return target.ready || Compose(view).ready;
            layer.included = visible;
            return Compose(view).ready; // Re-cache before forwarding
        }
        return false;
    }

    ClickNode Click(View view, std::uint32_t x, std::uint32_t y)
    {
        ClickNode result{};
        result.view = view;
        const BitmapCacheNode& target = Target(view);
        if (!target.ready || x >= target.width || y >= target.height) return result;
        const auto& sources = Sources(view);
        for (std::size_t i = target.layers.size(); i-- > 0;)
        {
            const auto& layer = target.layers[i];
            if (!layer.included || !layer.clickable) continue;
            // PNG alpha at the ORIGINAL canvas coordinate; never a rectangle hitbox.
            const std::uint64_t offset =
                (std::uint64_t(y) * target.width + x) * 4ULL + 3ULL;
            std::ifstream in{ sources[i].path, std::ios::binary };
            if (!in) continue;
            in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
            char alpha = 0;
            if (in.get(alpha) && static_cast<unsigned char>(alpha) != 0)
            {
                result.button = layer.name;
                result.ready = true;
                return result;
            }
        }
        return result;
    }
}


