#include "SAO_Toth.h"
#include "SAO_Prime_Cache_Node.h"
#include "SAO_Toth_Maximized_Overlay_Math.h"
#include "SAO_Toth_Minimized_Overlay_Math.h"
#include "SAO_Alphabet_Conductor.h"
#include <process.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <algorithm>
#include <mutex>
#include <vector>
#include <cstdint>
#include <cwctype>
#include <string_view>
#include <utility>

namespace SAO_Toth
{
    OutputNode Cache{};
    namespace
    {
        namespace Overlay = SAO_Toth_Overlay_Conductor;
        namespace fs = std::filesystem;
        constexpr std::uint64_t BYTES =
            std::uint64_t(SAO_Main_Screen_Conductor::WIDTH) *
            SAO_Main_Screen_Conductor::HEIGHT * 4ULL;

        std::mutex gate;
        HANDLE worker = nullptr;
        HANDLE preloadEvent = nullptr;
        HANDLE stopEvent = nullptr;
        HWND destination = nullptr;
        bool initialPrepared = false;
        bool started = false;

        fs::path CacheFolder()
        {
            wchar_t path[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
            if (!n || n >= MAX_PATH) return {};
            return fs::path(std::wstring(path, n)).parent_path() / L"Cache";
        }

        // Save files live beside the project's assets, never in the disposable
        // executable Cache and never in a redirected Desktop guess.
        fs::path PlayerFolder()
        {
            wchar_t executable[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
            if (!n || n >= MAX_PATH) return {};
            fs::path folder = fs::path(std::wstring(executable, n)).parent_path();
            for (int depth = 0; depth < 8; ++depth)
            {
                std::error_code ec;
                const fs::path assets = folder / L"SAO_I_E Assets";
                if (fs::is_directory(assets, ec) && !ec)
                    return assets / L"Player Saves";
                const fs::path parent = folder.parent_path();
                if (parent == folder || parent.empty()) break;
                folder = parent;
            }
            return {};
        }

        std::wstring TrimName(const std::wstring& text)
        {
            const auto first = text.find_first_not_of(L" \t\r\n");
            if (first == std::wstring::npos) return {};
            const auto last = text.find_last_not_of(L" \t\r\n");
            return text.substr(first, last - first + 1);
        }

        std::wstring LowerCommand(std::wstring input)
        {
            for (wchar_t& c : input) c = static_cast<wchar_t>(std::towlower(c));
            return input;
        }

        bool ValidPlayerName(const std::wstring& candidate)
        {
            if (candidate.empty() || candidate.size() > 64 ||
                candidate == L"." || candidate == L".." ||
                candidate.back() == L'.') return false;
            for (wchar_t c : candidate)
            {
                if (c < 32 || c == 127 ||
                    std::wstring_view(L"\\/:*?\"<>|#").find(c) != std::wstring_view::npos)
                    return false;
            }
            return true;
        }

        // Compact persistent UTF-16LE record: magic (8 bytes), length (u32),
        // then UTF-16 code units. No 1920x1080 bitmaps in player saves.
        constexpr char PROFILE_MAGIC[8] = { 'S','A','O','P','L','R','0','1' };

        bool WriteProfileFile(const fs::path& destination, const std::wstring& name)
        {
            if (!ValidPlayerName(name)) return false;
            const fs::path temporary = destination.wstring() + L".tmp";
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            out.write(PROFILE_MAGIC, 8);
            const std::uint32_t length = static_cast<std::uint32_t>(name.size());
            for (unsigned k = 0; k < 4; ++k) out.put(static_cast<char>((length >> (8 * k)) & 255));
            for (wchar_t c : name)
            {
                const std::uint16_t value = static_cast<std::uint16_t>(c);
                out.put(static_cast<char>(value & 255));
                out.put(static_cast<char>(value >> 8));
            }
            out.flush();
            const bool written = out.good();
            out.close();
            if (!written || !MoveFileExW(temporary.c_str(), destination.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            {
                DeleteFileW(temporary.c_str());
                return false;
            }
            return true;
        }

        bool ReadProfileFile(const fs::path& path, std::wstring& name)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in) return false;
            char magic[8]{};
            if (!in.read(magic, 8) || !std::equal(magic, magic + 8, PROFILE_MAGIC))
                return false;
            std::uint32_t length = 0;
            for (unsigned k = 0; k < 4; ++k)
            {
                const int v = in.get();
                if (v == EOF) return false;
                length |= static_cast<std::uint32_t>(v) << (8 * k);
            }
            if (!length || length > 64) return false;
            std::wstring recovered;
            recovered.reserve(length);
            for (std::uint32_t k = 0; k < length; ++k)
            {
                const int lo = in.get(), hi = in.get();
                if (lo == EOF || hi == EOF) return false;
                recovered.push_back(static_cast<wchar_t>(lo | (hi << 8)));
            }
            if (in.peek() != EOF || !ValidPlayerName(recovered)) return false;
            name = std::move(recovered);
            return true;
        }

        bool LoadActivePlayer(std::wstring& name)
        {
            const fs::path root = PlayerFolder();
            if (root.empty()) return false;
            std::wstring selected;
            if (!ReadProfileFile(root / L"SAO_Player_Active.cache", selected)) return false;
            std::wstring verified;
            if (!ReadProfileFile(root / (selected + L".cache"), verified) ||
                verified != selected) return false;
            name = std::move(verified);
            return true;
        }

        bool SaveActivePlayer(const std::wstring& name)
        {
            const fs::path root = PlayerFolder();
            if (root.empty() || !ValidPlayerName(name)) return false;
            std::error_code ec;
            fs::create_directories(root, ec);
            if (ec || !fs::is_directory(root)) return false;
            // Write the named profile FIRST, then select it as active.
            return WriteProfileFile(root / (name + L".cache"), name) &&
                WriteProfileFile(root / L"SAO_Player_Active.cache", name);
        }

        void WelcomePlayer()
        {
            Cache.playerName.clear();
            Cache.awaitingPlayerName = false;
            if (LoadActivePlayer(Cache.playerName))
            {
                Cache.tothText = L"Hello, " + Cache.playerName +
                    L"! What would you like to do today?";
                Cache.bastetText = L"Welcome back.";
            }
            else
            {
                Cache.tothText = L"Hello, my name is Toth. What is your name?";
                Cache.bastetText = L"Enter #Player Name, then #Your Full Name.";
            }
        }

        bool Usable(const fs::path& path)
        {
            std::error_code ec;
            return fs::is_regular_file(path, ec) && !ec &&
                fs::file_size(path, ec) == BYTES && !ec;
        }

        // The *Location PNGs* define text bounds. The scan runs once at
        // Toth preload, not on mouse, typing or animated Prime frames.
        TextLocation Location(const std::wstring& asset, bool minimized = false,
            bool namePlate = false)
        {
            TextLocation box{};
            const fs::path path = CacheFolder() /
                (minimized ? L"SAO_Toth_Minimized_Overlay_Math" :
                    L"SAO_Toth_Maximized_Overlay_Math") / (asset + L".png.cache");
            std::ifstream in(path, std::ios::binary);
            if (!in) return box;
            const int w = SAO_Main_Screen_Conductor::WIDTH;
            const int h = SAO_Main_Screen_Conductor::HEIGHT;
            std::vector<unsigned char> row(static_cast<std::size_t>(w) * 4);
            int minX = w, minY = h, maxX = -1, maxY = -1;
            for (int y = 0; y < h; ++y)
            {
                if (!in.read(reinterpret_cast<char*>(row.data()), row.size())) return {};
                for (int x = 0; x < w; ++x)
                {
                    // The player-name location is intentionally near-transparent.
                    // For that slot, even alpha 1 is a meaningful marker.
                    const unsigned char alpha = row[static_cast<std::size_t>(x) * 4 + 3];
                    if (alpha < (namePlate ? 1 : 12)) continue;
                    minX = (std::min)(minX, x);
                    maxX = (std::max)(maxX, x);
                    minY = (std::min)(minY, y);
                    maxY = (std::max)(maxY, y);
                }
            }
            if (maxX < minX || maxY < minY) return box;
            // Name location pixels may describe a very faint, narrow band.
            // Use the PNG-defined coordinates without eating 24 pixels of it.
            const int margin = namePlate ? 2 : 12;
            box.x = minX + margin;
            box.y = minY + margin;
            box.width = (std::max)(0, maxX - minX + 1 - 2 * margin);
            box.height = (std::max)(0, maxY - minY + 1 - 2 * margin);
            if (namePlate && box.width > 10 && box.height < 24)
            {
                // A very thin location marker still defines the text center.
                const int center = (minY + maxY) / 2;
                box.y = (std::max)(0, center - 16);
                box.height = (std::min)(32, h - box.y);
            }
            box.ready = box.width > 10 && box.height > 10;
            return box;
        }

        void PrepareLocations()
        {
            Cache.inputLocation = Location(L"SAO_Toth_Player_Input_Location");
            Cache.minimizedInputLocation = Location(L"SAO_Toth_Minimized_Input_Location", true);
            Cache.tothOutputLocation = Location(L"SAO_Toth_Toth_Output_Location");
            Cache.bastetOutputLocation = Location(L"SAO_Toth_Baetet_Output_Location");
            Cache.playerNameLocation = Location(L"SAO_Toth_Player_Name_Location", false, true);
            if (!Cache.bastetOutputLocation.ready)
                Cache.bastetOutputLocation = Location(L"SAO_Toth_Bastet_Output_Location");
        }

        // FIRST-FRAME FAST PATH: forward the previous completed overlay cache
        // immediately, without running 70 Math checks or composing 63 layers.
        // Validate all sources after the Program's two-second preload event.
        bool CachedStart()
        {
            const fs::path root = CacheFolder();
            if (root.empty()) return false;
            const fs::path conductor = root / L"SAO_Toth_Overlay_Conductor" /
                L"SAO_Toth_Maximized_Overlay.cache";
            const fs::path oldToth = root / L"SAO_Toth" /
                L"SAO_Toth_Maximized.cache";
            const fs::path& cached = Usable(conductor) ? conductor : oldToth;
            if (!Usable(cached)) return false;
            Cache.view = Overlay::View::Maximized;
            Cache.overlayPath = cached.wstring();
            Cache.maximizedPath = cached.wstring();
            // If a minimized cache already exists, reuse it on an early click.
            const fs::path reduced = root / L"SAO_Toth_Overlay_Conductor" /
                L"SAO_Toth_Minimized_Overlay.cache";
            if (Usable(reduced)) Cache.minimizedPath = reduced.wstring();
            Cache.width = SAO_Main_Screen_Conductor::WIDTH;
            Cache.height = SAO_Main_Screen_Conductor::HEIGHT;
            Cache.ready = true;
            Cache.error.clear();
            Cache.validated = false;
            ++Cache.revision;
            return true;
        }

        bool Prepare()
        {
            const auto& max = SAO_Toth_Maximized_Overlay_Math::Build();
            if (!max.ready)
            {
                Cache.error = max.error.empty() ?
                    L"Toth Maximized Math is not ready." : max.error;
                return false;
            }
            const auto& min = SAO_Toth_Minimized_Overlay_Math::Build();
            if (!min.ready)
            {
                Cache.error = min.error.empty() ?
                    L"Toth Minimized Math is not ready." : min.error;
                return false;
            }
            const auto& combined = Overlay::Build(max, min, Cache.view);
            if (!combined.ready)
            {
                Cache.error = combined.error.empty() ?
                    L"Toth Overlay Conductor is not ready." : combined.error;
                return false;
            }
            // The second view is prepared only once during the same Math
            // preload; subsequent clicks never rebuild either view.
            const auto other = Cache.view == Overlay::View::Maximized ?
                Overlay::View::Minimized : Overlay::View::Maximized;
            if (!Overlay::Compose(other).ready)
            {
                Cache.error = Overlay::Cache.error;
                return false;
            }
            initialPrepared = true;
            Cache.maximizedPath = Overlay::Cache.maximized.path;
            Cache.minimizedPath = Overlay::Cache.minimized.path;
            return true;
        }

        bool Receive(Overlay::View view)
        {
            const auto& image = view == Overlay::View::Maximized ?
                Overlay::Cache.maximized : Overlay::Cache.minimized;
            if (!Overlay::Cache.ready || !image.ready)
            {
                Cache.error = Overlay::Cache.error.empty() ?
                    L"Toth requested bitmap is not prepared." : Overlay::Cache.error;
                return false;
            }
            Cache.view = view;
            Cache.overlayPath = image.path; // Reference, not another bitmap copy.
            Cache.maximizedPath = Overlay::Cache.maximized.path;
            Cache.minimizedPath = Overlay::Cache.minimized.path;
            Cache.width = image.width;
            Cache.height = image.height;
            Cache.error.clear();
            Cache.ready = true;
            ++Cache.revision;
            return true;
        }

        bool Forward()
        {
            const bool result = SAO_Prime_Cache_Node::ReceiveToth(Cache, destination);
            Cache.backRequested = false; // One forwarded event, not a lasting state.
            Cache.startGameRequested = false;
            Cache.harvestTallGrassRequested = false;
            Cache.inventoryRequested = false;
            Cache.gearRequested = false;
            return result;
        }

        unsigned __stdcall Run(void*)
        {
            {
                std::lock_guard<std::mutex> lock(gate);
                // Cold start requires the first composite once. Warm start
                // delivers the finished image immediately, BEFORE validation.
                const bool warm = CachedStart();
                if (!warm)
                {
                    Cache.ready = Prepare() && Receive(Overlay::View::Maximized);
                }
                Forward();
            }

            const HANDLE events[2] = { stopEvent, preloadEvent };
            if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                return 0;

            // Source validation and on-demand minimized composition are NOT
            // requirements of the first frame. Program triggers this once at 2s.
            {
                std::lock_guard<std::mutex> lock(gate);
                if (WaitForSingleObject(stopEvent, 0) == WAIT_OBJECT_0) return 0;
                if (!initialPrepared)
                {
                    const std::wstring before = Cache.overlayPath;
                    std::error_code stampError;
                    const auto beforeTime = before.empty() ? fs::file_time_type{} :
                        fs::last_write_time(fs::path(before), stampError);
                    const bool hadTime = !before.empty() && !stampError;
                    const bool success = Prepare();
                    if (!success)
                    {
                        // Keep a valid previously displayed bitmap when possible.
                        // Forward exactly once below after validation finishes.
                    }
                    else
                    {
                        const auto& current = Cache.view == Overlay::View::Maximized ?
                            Overlay::Cache.maximized : Overlay::Cache.minimized;
                        // Detect a rebuilt conductor bitmap even when its path
                        // stays the same. Unchanged caches are not forwarded.
                        stampError.clear();
                        const auto afterTime = fs::last_write_time(
                            fs::path(current.path), stampError);
                        const bool changed = before != current.path ||
                            (!stampError && hadTime && afterTime != beforeTime);
                        if (current.ready && changed)
                        {
                            Receive(Cache.view);
                        }
                    }
                }
                // Preload has completed once. No combined 360-frame rebuild:
                // both static view caches are now available to the shared node.
                Cache.validated = initialPrepared;
                // Before game animation is changed, forward the normal result.
                Forward();
            }
            // Independent English preload. Never blocks first game frame.
            {
                std::lock_guard<std::mutex> lock(gate);
                PrepareLocations();
                ++Cache.revision;
                Forward();
            }
            return 0;
        }

        // The minimized Enter PNG is now an outlined glowing button.
        // Give its visible alpha pixels a small radius so transparent gaps
        // beside the glow are clickable. No manually positioned rectangle.
        // Tested only on mouse clicks, never during animation/preload.
        bool ButtonPixel(Overlay::View view, std::uint32_t x, std::uint32_t y)
        {
            constexpr int ENTER_CLICK_RADIUS = 48;  // pixels around actual PNG glow
            constexpr unsigned char MIN_GLOW_ALPHA = 8;
            const int width = static_cast<int>(SAO_Main_Screen_Conductor::WIDTH);
            const int height = static_cast<int>(SAO_Main_Screen_Conductor::HEIGHT);
            if (x >= static_cast<std::uint32_t>(width) ||
                y >= static_cast<std::uint32_t>(height)) return false;
            const bool minimized = view == Overlay::View::Minimized;
            const fs::path path = CacheFolder() /
                (minimized ? L"SAO_Toth_Minimized_Overlay_Math" :
                    L"SAO_Toth_Maximized_Overlay_Math") /
                (minimized ? L"SAO_Toth_Minimized_Input_Enter_Button.png.cache" :
                    L"SAO_Toth_Minimize_Button.png.cache");
            std::ifstream in(path, std::ios::binary);
            if (!in) return false;

            if (!minimized)
            {
                const std::uint64_t offset =
                    (std::uint64_t(y) * width + x) * 4ULL + 3;
                in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
                char alpha = 0;
                return in.get(alpha) && static_cast<unsigned char>(alpha) != 0;
            }

            const int cx = static_cast<int>(x);
            const int cy = static_cast<int>(y);
            const int left = (cx > ENTER_CLICK_RADIUS) ? cx - ENTER_CLICK_RADIUS : 0;
            const int right = (cx + ENTER_CLICK_RADIUS < width) ?
                cx + ENTER_CLICK_RADIUS : width - 1;
            const int top = (cy > ENTER_CLICK_RADIUS) ? cy - ENTER_CLICK_RADIUS : 0;
            const int bottom = (cy + ENTER_CLICK_RADIUS < height) ?
                cy + ENTER_CLICK_RADIUS : height - 1;
            const std::size_t rowBytes = static_cast<std::size_t>(right - left + 1) * 4;
            std::vector<char> row(rowBytes);
            for (int py = top; py <= bottom; ++py)
            {
                const std::uint64_t offset =
                    (std::uint64_t(py) * width + left) * 4ULL;
                in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
                if (!in.read(row.data(), static_cast<std::streamsize>(rowBytes)))
                    return false;
                const int dy = py - cy;
                for (int px = left; px <= right; ++px)
                {
                    const int dx = px - cx;
                    if (dx * dx + dy * dy > ENTER_CLICK_RADIUS * ENTER_CLICK_RADIUS)
                        continue;
                    if (static_cast<unsigned char>(row[static_cast<std::size_t>(px - left) * 4 + 3])
                        >= MIN_GLOW_ALPHA)
                        return true;
                }
            }
            return false;
        }

        // The Math has already cached every PNG. Clicks only inspect the
        // existing per-button bitmap alpha and forward the selected icon path.
        struct IconPair
        {
            const wchar_t* button;
            const wchar_t* icon;
            bool research;
        };

        constexpr IconPair ICON_BUTTONS[] =
        {
            { L"SAO_Toth_Research_Language_Button",      L"SAO_Toth_Research_Language_Icon",      true },
            { L"SAO_Toth_Research_Mathematics_Button",   L"SAO_Toth_Research_Mathematics_Icon",   true },
            { L"SAO_Toth_Research_Technology_Button",    L"SAO_Toth_Research_Technology_Icon",    true },
            { L"SAO_Toth_Research_History_Button",       L"SAO_Toth_Research_History_Icon",       true },
            { L"SAO_Toth_Research_World_Lore_Button",    L"SAO_Toth_Research_World_Lore_Icon",    true },
            { L"SAO_Toth_Player_Wisdom_Button",          L"SAO_Toth_Player_Wisdom_Icon",          false },
            { L"SAO_Toth_Player_Intelligence_Button",    L"SAO_Toth_Player_Intelligence_Icon",    false },
            { L"SAO_Toth_Player_Practice_Button",        L"SAO_Toth_Player_Practice_Icon",        false },
            { L"SAO_Toth_Player_Discipline_Button",      L"SAO_Toth_Player_Discipline_Icon",      false },
            { L"SAO_Toth_Player_Application_Button",     L"SAO_Toth_Player_Application_Icon",     false },
            { L"SAO_Toth_Player_Physique_Button",        L"SAO_Toth_Player_Physique_Icon",        false }
        };

        bool CachedButtonPixel(const fs::path& path, std::uint32_t x, std::uint32_t y)
        {
            if (x >= SAO_Main_Screen_Conductor::WIDTH ||
                y >= SAO_Main_Screen_Conductor::HEIGHT) return false;
            std::ifstream bitmap(path, std::ios::binary);
            if (!bitmap) return false;
            const std::uint64_t offset =
                (std::uint64_t(y) * SAO_Main_Screen_Conductor::WIDTH + x) * 4 + 3;
            bitmap.seekg(static_cast<std::streamoff>(offset));
            char alpha = 0;
            return bool(bitmap.get(alpha)) && static_cast<unsigned char>(alpha) > 0;
        }

        bool SelectLargeIcon(std::uint32_t x, std::uint32_t y)
        {
            if (Cache.view != Overlay::View::Maximized) return false;
            const fs::path root = CacheFolder() / L"SAO_Toth_Maximized_Overlay_Math";
            for (const IconPair& item : ICON_BUTTONS)
            {
                const fs::path button = root / (std::wstring(item.button) + L".png.cache");
                if (!CachedButtonPixel(button, x, y)) continue;
                const fs::path icon = root / (std::wstring(item.icon) + L".png.cache");
                if (!Usable(icon)) return false;
                std::wstring& selected = item.research ?
                    Cache.selectedResearchIcon : Cache.selectedPlayerIcon;
                const bool wasProgress = Cache.progressSelected; Cache.progressSelected = false;
                if (selected == icon.wstring() && !wasProgress) return true;
                selected = icon.wstring();
                ++Cache.revision;
                Forward(); // Toth -> shared node; no Math/Conductor back-call.
                return true;
            }
            return false;
        }

        // State change only. Both views reuse existing caches; no preload signal.
        bool ChangeView(Overlay::View next)
        {
            if (Cache.view == next) return true;
            if (initialPrepared)
            {
                // Both views were composed at preload: switch, do NOT compose again.
                if (!Receive(next)) return false;
            }
            else
            {
                // Warm start before the two-second preload: use the existing
                // completed composite if present; no Math/Conductor rebuild.
                const fs::path root = CacheFolder();
                const fs::path cached = root / L"SAO_Toth_Overlay_Conductor" /
                    (next == Overlay::View::Minimized ?
                        L"SAO_Toth_Minimized_Overlay.cache" :
                        L"SAO_Toth_Maximized_Overlay.cache");
                const fs::path old = root / L"SAO_Toth" /
                    (next == Overlay::View::Minimized ?
                        L"SAO_Toth_Minimized.cache" :
                        L"SAO_Toth_Maximized.cache");
                const fs::path& selected = Usable(cached) ? cached : old;
                if (Usable(selected))
                {
                    Cache.view = next;
                    Cache.overlayPath = selected.wstring();
                    if (next == Overlay::View::Minimized)
                        Cache.minimizedPath = selected.wstring();
                    else
                        Cache.maximizedPath = selected.wstring();
                    ++Cache.revision;
                }
                else if (!Prepare() || !Receive(next)) return false;
            }
            return true;
        }

    }

    bool Start(HWND target)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (started) return true;
        destination = target;
        WelcomePlayer(); // Load only one small persistent record before first frame.
        stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        preloadEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (stopEvent && preloadEvent)
            worker = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Run, nullptr, 0, nullptr));
        if (!worker)
        {
            if (stopEvent) CloseHandle(stopEvent);
            if (preloadEvent) CloseHandle(preloadEvent);
            stopEvent = preloadEvent = nullptr;
            Cache.ready = false;
            Cache.error = L"Cannot start the Toth cache-preparation worker.";
            SAO_Prime_Cache_Node::ReceiveToth(Cache, target);
            return false;
        }
        started = true;
        return true;
    }

    void Preload()
    {
        // One signal from the PROGRAM's existing PRELOAD timer; not a Toth clock.
        if (preloadEvent) SetEvent(preloadEvent);
    }

    bool Click(std::uint32_t x, std::uint32_t y, HWND target)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (!Cache.ready) return false;
        if (Cache.view == Overlay::View::Maximized && !Cache.inputText.empty())
        {
            // UI event goes FORWARD to Program, where user input ENTERS Alphabet.
            // Toth never queries the Alphabet or any preceding Math.
            const fs::path button = CacheFolder() / L"SAO_Toth_Maximized_Overlay_Math" /
                L"SAO_Toth_Player_Input_Enter_Button.png.cache";
            std::ifstream file(button, std::ios::binary);
            if (file && x < SAO_Main_Screen_Conductor::WIDTH &&
                y < SAO_Main_Screen_Conductor::HEIGHT)
            {
                const std::uint64_t offset =
                    (std::uint64_t(y) * SAO_Main_Screen_Conductor::WIDTH + x) * 4ULL + 3;
                file.seekg(static_cast<std::streamoff>(offset));
                char alpha = 0;
                if (file.get(alpha) && static_cast<unsigned char>(alpha) != 0)
                    return PostMessageW(target, WM_APP + 9, 0, 0) != FALSE;
            }
        }
        // Category/attribute selection updates a single static cache view,
        // without changing the overlay PNGs or the Prime animation.
        if (Cache.view == Overlay::View::Maximized && CachedButtonPixel(
            CacheFolder() / L"SAO_Toth_Maximized_Overlay_Math" / L"SAO_Toth_Progress_Button.png.cache", x, y)) {
            Cache.progressSelected = !Cache.progressSelected;
            ++Cache.revision; destination = target; return Forward();
        }
        if (SelectLargeIcon(x, y)) return true;

        // With typed input, minimized Enter submits it instead of blindly restoring.
        // With an empty input, its original PNG click still restores Toth.
        if (Cache.view == Overlay::View::Minimized && !Cache.inputText.empty() &&
            ButtonPixel(Overlay::View::Minimized, x, y))
            return PostMessageW(target, WM_APP + 9, 0, 0) != FALSE;

        const Overlay::View oldView = Cache.view;
        const Overlay::View next = oldView == Overlay::View::Maximized ?
            Overlay::View::Minimized : Overlay::View::Maximized;
        const wchar_t* expected = oldView == Overlay::View::Maximized ?
            L"SAO_Toth_Minimize_Button.png" :
            L"SAO_Toth_Minimized_Input_Enter_Button.png";
        // Outlined minimized Enter uses its PNG glow with a small hit radius
        // both before and after preload. Maximize's existing PNG test stays exact.
        const bool hit = oldView == Overlay::View::Minimized ?
            ButtonPixel(oldView, x, y) :
            (initialPrepared ? Overlay::Click(oldView, x, y).button == expected :
                ButtonPixel(oldView, x, y));
        if (!hit) return false;

        if (!ChangeView(next)) return false;
        destination = target;
        return Forward(); // State forwarding ONLY. Never a new preload request.
    }

    // Alphabet -> Toth -> shared cache. Only receive completed results.
    bool ReceiveAlphabet(const SAO_Alphabet_Conductor::OutputNode& language)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (language.revision < Cache.alphabetRevision) return true;
        Cache.alphabetRevision = language.revision;
        const bool newAnswer = language.answerRevision > Cache.answerRevision;
        Cache.answerRevision = language.answerRevision;
        Cache.englishReady = language.ready;
        Cache.inputText = language.inputText;
        // Ordinary Alphabet startup and typing must never erase a greeting.
        // Only a submitted answer changes Toth's dialogue. Source failures
        // remain visible in Bastet's supporting output.
        if (newAnswer)
        {
            Cache.backRequested = false;
            Cache.startGameRequested = false;
            Cache.harvestTallGrassRequested = false;
            Cache.inventoryRequested = false;
            Cache.gearRequested = false;
            const std::wstring command = TrimName(language.submittedCommand);
            const std::wstring normalized = LowerCommand(command);
            const bool wasProgress = Cache.progressSelected;
            if (!normalized.empty())Cache.progressSelected = false;
            if (normalized == L"#progress") {
                Cache.progressSelected = true;
                ChangeView(Overlay::View::Maximized);
            }
            else if (normalized == L"#back" && wasProgress) {
                Cache.tothText = L"Progress closed."; Cache.bastetText.clear();
            }
            else if (normalized == L"#back")
            {
                Cache.backRequested = true;
                Cache.tothText = L"Returning to Main.";
                Cache.bastetText.clear();
            }
            else if (normalized == L"#start_game")
            {
                Cache.startGameRequested = true;
                Cache.tothText = L"Opening Settlement.";
                Cache.bastetText.clear();
            }
            else if (normalized == L"#harvest_tall_grass")
            {
                Cache.harvestTallGrassRequested = true;
                Cache.tothText = L"Opening Tall Grass / Straw Harvest.";
                Cache.bastetText.clear();
            }
            else if (normalized == L"#inventory" || normalized == L"#gear")
            {
                Cache.inventoryRequested = normalized == L"#inventory";
                Cache.gearRequested = normalized == L"#gear";
                // Reuse prepared minimized Toth overlay. Both new panels are static.
                ChangeView(Overlay::View::Minimized);
                Cache.tothText = L"Inventory and Gear ready.";
                Cache.bastetText.clear();
            }
            else if (normalized == L"#player name")
            {
                Cache.awaitingPlayerName = true;
                Cache.tothText = L"What is your player name? Enter # followed by your full name.";
                Cache.bastetText = L"Your player name will be used for persistent saves.";
            }
            else if (Cache.awaitingPlayerName && !command.empty() &&
                command.front() == L'#' && normalized != L"#maximize" &&
                normalized != L"#minimize" && normalized != L"#exit_game")
            {
                const std::wstring candidate = TrimName(command.substr(1));
                if (!ValidPlayerName(candidate))
                {
                    Cache.tothText = L"That player name is invalid. Please try #Your Full Name.";
                    Cache.bastetText = L"Use a name of 1 to 64 characters without filename symbols.";
                }
                else if (!SaveActivePlayer(candidate))
                {
                    Cache.tothText = L"I could not save the player profile.";
                    Cache.bastetText = L"Check the SAO_I_E Assets\\Player Saves folder.";
                }
                else
                {
                    Cache.playerName = candidate;
                    Cache.awaitingPlayerName = false;
                    Cache.tothText = L"Hello, " + Cache.playerName +
                        L"! What would you like to do today?";
                    Cache.bastetText = L"Your player profile has been saved.";
                }
            }
            else if (normalized == L"#minimize")
                ChangeView(Overlay::View::Minimized);
            else if (normalized == L"#maximize")
                ChangeView(Overlay::View::Maximized);
            else if (normalized == L"#exit_game")
                Cache.exitRequested = true;
            else if (!command.empty())
            {
                Cache.tothText = L"Unknown command. Use #Player Name to register or change names.";
                Cache.bastetText = L"Other commands remain available.";
            }
            else
            {
                Cache.tothText = language.tothText;
                Cache.bastetText = language.bastetText;
            }
        }
        else if (!language.ready && !language.error.empty())
        {
            Cache.bastetText = language.error;
        }

        if (newAnswer && language.submittedCommand.empty() && Cache.inputText.empty() &&
            Cache.view == Overlay::View::Minimized)
        {
            // A submitted reply opens Toth again using its existing static view cache.
            const fs::path cached = CacheFolder() / L"SAO_Toth_Overlay_Conductor" /
                L"SAO_Toth_Maximized_Overlay.cache";
            if (initialPrepared && Overlay::Cache.maximized.ready)
                Receive(Overlay::View::Maximized);
            else if (Usable(cached))
            {
                Cache.view = Overlay::View::Maximized;
                Cache.overlayPath = cached.wstring();
            }
        }
        ++Cache.revision;
        return !Cache.ready || Forward(); // Language can arrive before first visual frame.
    }

    void Stop()
    {
        if (stopEvent) SetEvent(stopEvent);
        if (preloadEvent) SetEvent(preloadEvent);
        if (worker)
        {
            WaitForSingleObject(worker, INFINITE);
            CloseHandle(worker);
        }
        if (preloadEvent) CloseHandle(preloadEvent);
        if (stopEvent) CloseHandle(stopEvent);
        worker = stopEvent = preloadEvent = nullptr;
        started = false;
        initialPrepared = false;
        destination = nullptr;
    }
}




