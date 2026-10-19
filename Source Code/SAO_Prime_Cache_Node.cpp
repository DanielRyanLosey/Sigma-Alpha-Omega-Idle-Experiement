#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_Prime_Cache_Node.h"
#include "SAO_Gear_Inventory_Conductor.h"
#include "SAO_Progression_Conductor.h"
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <array>
#include <cmath>
#include <limits>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// Prime owns the moving game bitmap. Toth owns its two static bitmap caches.
// This final node presents the game and retains ONE separate static Windows
// alpha layer for Toth. There is NO Toth blend in the 33-ms animation path.
namespace SAO_Prime_Cache_Node
{
    OutputNode Cache{};
    namespace
    {
        constexpr UINT WIDTH = SAO_Main_Screen_Conductor::WIDTH;
        constexpr UINT HEIGHT = SAO_Main_Screen_Conductor::HEIGHT;
        constexpr DWORD BYTES = SAO_Main_Screen_Conductor::BYTES;
        constexpr UINT FRAMES = SAO_Main_Screen_Conductor::FRAMES;

        std::mutex gate;
        SAO_Toth::OutputNode tothState{};
        bool tothReceived = false;
        bool preloadRequested = false;
        bool tothPreloadIssued = false;
        HWND destination = nullptr;

        // Game presentation is the SAME DIB passed forward from Prime.
        HDC presentation = nullptr;
        HGDIOBJ presentationOriginal = nullptr;

        // One static alpha overlay, updated only when Toth's view changes.
        HWND staticToth = nullptr;
        std::uint64_t displayedRevision = 0;
        bool staticShown = false;
        std::uint64_t displayedInventoryRevision = 0;
        std::uint64_t displayedProgressRevision = 0;
        bool settlementSelected = false;
        bool harvestSelected = false;
        bool startPending = false;
        bool harvestPending = false;
        bool inventoryPending = false;

        // Settlement Back is already forwarded in the Game grouping.
        // Keep only its visible pixels so each Harvest animation frame can
        // add the same cached layer without another bitmap or PNG decode.
        struct BackPixel
        {
            std::uint32_t offset = 0;
            BYTE bgra[4]{};
        };
        std::vector<BackPixel> harvestBackPixels;
        std::wstring harvestBackPath;

        // A forwarded Character Prime cache, flattened once to visible pixels.
        // The 33-ms path never decodes PNGs or reads a second full image.
        std::array<std::vector<BackPixel>, SAO_Character_Harvest::COUNT> characterPixels;
        std::array<bool, SAO_Character_Harvest::COUNT> characterPrepared{};
        std::array<std::vector<BackPixel>, SAO_Character_Harvest::COUNT> oakToolPixels;
        std::array<std::wstring, SAO_Character_Harvest::COUNT> oakToolPaths;

        // The 23-pose Character stream is continuous whenever Harvest is
        // selected. A mouse click never starts, stops, or restarts this sequence.
        UINT actionFrame = 0;
        bool restoreBushelsNextTick = false;

        // One forwarded, PRECOMPOSED bushel overlay for the active stage.
        // Changing a stage reads its cache once; normal ticks never rebuild
        // ten Grass/Straw Math layers or touch their source files.
        std::wstring bushelStagePath;
        std::vector<BackPixel> bushelStagePixels;


        bool Read(const std::wstring& path, BYTE* pixels)
        {
            FILE* file = nullptr;
            if (!pixels || _wfopen_s(&file, path.c_str(), L"rb") != 0 || !file)
                return false;
            const bool ok = fread(pixels, 1, BYTES, file) == BYTES;
            return fclose(file) == 0 && ok;
        }

        // Only one cached alphabet size is selected for the entire name.
        // No Character Math Build() calls, PNG decoding or work on animation ticks.
        namespace fs = std::filesystem;
        struct PlayerGlyph
        {
            int width = 0;
            int height = 0;
            std::vector<BYTE> pixels;
        };
        using GlyphKey = std::pair<int, wchar_t>; // Size 40/80/160 and character.
        std::map<GlyphKey, PlayerGlyph> playerGlyphs;

        fs::path ExecutableFolder()
        {
            wchar_t executable[MAX_PATH]{};
            const DWORD count = GetModuleFileNameW(nullptr, executable, MAX_PATH);
            if (!count || count >= MAX_PATH) return {};
            return fs::path(std::wstring(executable, count)).parent_path();
        }

        fs::path AssetRoot()
        {
            fs::path folder = ExecutableFolder();
            for (int depth = 0; depth < 8 && !folder.empty(); ++depth)
            {
                std::error_code ec;
                const fs::path assets = folder / L"SAO_I_E Assets";
                if (fs::is_directory(assets, ec) && !ec) return assets;
                const fs::path parent = folder.parent_path();
                if (parent == folder) break;
                folder = parent;
            }
            return {};
        }

        // The Maths already own these three sizes. Only READ their caches.
        const wchar_t* SizeLabel(int size)
        {
            if (size == 40) return L"Small";
            if (size == 80) return L"Medium";
            if (size == 160) return L"Large";
            return nullptr;
        }

        fs::path PlayerGlyphCachePath(wchar_t c, int size)
        {
            const wchar_t* label = SizeLabel(size);
            if (!label) return {};
            const fs::path root = ExecutableFolder() / L"Cache";
            if (c >= L'A' && c <= L'Z')
                return root / (std::wstring(L"SAO_Upper_") + label + L"_Math") /
                (L"SAO_Upper_" + std::wstring(1, c) + L"_" + label + L".png.cache");
            if (c >= L'a' && c <= L'z')
                return root / (std::wstring(L"SAO_Lower_") + label + L"_Math") /
                (L"SAO_Lower_" + std::wstring(1, c) + L"_" + label + L".png.cache");
            return {};
        }

        // Raw bitmap caches have no width/height header. When needed, read
        // only the PNG IHDR for dimensions. The pixels always come from Math cache.
        bool PlayerGlyphDimensions(wchar_t c, int size, std::uint64_t byteCount,
            std::uint32_t& width, std::uint32_t& height)
        {
            if (byteCount == static_cast<std::uint64_t>(BYTES))
            {
                width = WIDTH;
                height = HEIGHT;
                return true;
            }
            const wchar_t* label = SizeLabel(size);
            const fs::path assets = AssetRoot();
            if (label && !assets.empty())
            {
                const bool upper = c >= L'A' && c <= L'Z';
                const std::wstring filename = (upper ? L"SAO_Upper_" : L"SAO_Lower_") +
                    std::wstring(1, c) + L"_" + label + L".png";
                const fs::path png = assets / L"Alphabet" /
                    (upper ? L"Uppercase" : L"Lowercase") /
                    ((upper ? std::wstring(L"Upper ") : std::wstring(L"Lower ")) + label) /
                    filename;
                std::ifstream file(png, std::ios::binary);
                unsigned char header[24]{};
                if (file.read(reinterpret_cast<char*>(header), sizeof(header)) &&
                    header[0] == 137 && header[1] == 80 && header[2] == 78 &&
                    header[3] == 71 && header[12] == 'I' && header[13] == 'H' &&
                    header[14] == 'D' && header[15] == 'R')
                {
                    auto bigEndian = [&](int start) -> std::uint32_t
                    {
                        return (std::uint32_t(header[start]) << 24) |
                            (std::uint32_t(header[start + 1]) << 16) |
                            (std::uint32_t(header[start + 2]) << 8) |
                            std::uint32_t(header[start + 3]);
                    };
                    const std::uint32_t w = bigEndian(16), h = bigEndian(20);
                    if (w && h && std::uint64_t(w) * h * 4 == byteCount)
                    {
                        width = w;
                        height = h;
                        return true;
                    }
                }
            }
            if (byteCount % 4) return false;
            const auto side = static_cast<std::uint64_t>(
                std::sqrt(static_cast<double>(byteCount / 4)));
            if (side > 0 && side <= 16384 && side * side * 4 == byteCount)
            {
                width = height = static_cast<std::uint32_t>(side);
                return true;
            }
            return false;
        }

        bool LoadPlayerGlyph(wchar_t c, int size)
        {
            const GlyphKey key{ size, c };
            if (playerGlyphs.find(key) != playerGlyphs.end()) return true;
            const fs::path path = PlayerGlyphCachePath(c, size);
            if (path.empty()) return false;
            std::error_code error;
            const std::uint64_t bytes = fs::file_size(path, error);
            if (error || !bytes || bytes > 256ULL * 1024 * 1024 || bytes % 4)
                return false;
            std::uint32_t width = 0, height = 0;
            if (!PlayerGlyphDimensions(c, size, bytes, width, height)) return false;
            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            std::vector<BYTE> full(static_cast<std::size_t>(bytes));
            if (!file.read(reinterpret_cast<char*>(full.data()),
                static_cast<std::streamsize>(full.size()))) return false;

            int left = static_cast<int>(width), top = static_cast<int>(height);
            int right = -1, bottom = -1;
            for (std::uint32_t y = 0; y < height; ++y)
                for (std::uint32_t x = 0; x < width; ++x)
                {
                    const std::size_t offset = (std::size_t(y) * width + x) * 4;
                    if (full[offset + 3] < 12) continue;
                    left = (std::min)(left, static_cast<int>(x));
                    right = (std::max)(right, static_cast<int>(x));
                    top = (std::min)(top, static_cast<int>(y));
                    bottom = (std::max)(bottom, static_cast<int>(y));
                }
            if (right < left || bottom < top) return false;

            const int cropW = right - left + 1;
            const int cropH = bottom - top + 1;
            // One uniform glyph height for every letter from this size family.
            // Keep its original aspect ratio, rather than independently choosing
            // a different pixel height for each character.
            PlayerGlyph result;
            result.height = size;
            result.width = (std::max)(1, static_cast<int>(std::lround(
                static_cast<double>(cropW) * size / cropH)));
            result.width = (std::min)(result.width, size * 4);
            result.pixels.resize(std::size_t(result.width) * result.height * 4);
            for (int y = 0; y < result.height; ++y)
                for (int x = 0; x < result.width; ++x)
                {
                    const int sx = left + (std::min)(cropW - 1,
                        static_cast<int>((std::int64_t(x) * cropW) / result.width));
                    const int sy = top + (std::min)(cropH - 1,
                        static_cast<int>((std::int64_t(y) * cropH) / result.height));
                    const std::size_t source = (std::size_t(sy) * width + sx) * 4;
                    const std::size_t target = (std::size_t(y) * result.width + x) * 4;
                    std::memcpy(result.pixels.data() + target, full.data() + source, 4);
                }
            playerGlyphs.emplace(key, std::move(result));
            return true;
        }

        // Decide ONCE using the nameplate height and total character count.
        // All characters then come from the same 40 / 80 / 160 Math family.
        int PlayerNameSize(const SAO_Toth::TextLocation& box, const std::wstring& name)
        {
            int letters = 0, spaces = 0;
            for (wchar_t c : name)
                if (c == L' ') ++spaces;
                else ++letters;
            const double units = (std::max)(1.0, letters * 0.68 + spaces * 0.30);
            const double available = (std::min)(box.height * 0.85,
                box.width / units);
            if (available >= 120.0) return 160;
            if (available >= 60.0) return 80;
            return 40;
        }

        bool DrawPlayerNameGlyphs(BYTE* canvas,
            const SAO_Toth::TextLocation& box,
            const std::wstring& name)
        {
            if (!canvas || !box.ready || name.empty() ||
                box.width <= 0 || box.height <= 0) return false;
            const int chosenSize = PlayerNameSize(box, name);
            struct Placed { const PlayerGlyph* glyph = nullptr; double advance = 0.0; };
            std::vector<Placed> characters;
            characters.reserve(name.size());
            bool missingGlyph = false;
            double lineWidth = 0.0;
            const double spacing = chosenSize * 0.055;
            const double spaceWidth = chosenSize * 0.32;
            for (wchar_t c : name)
            {
                const PlayerGlyph* glyph = nullptr;
                double advance = spaceWidth;
                if (c != L' ' && LoadPlayerGlyph(c, chosenSize))
                {
                    glyph = &playerGlyphs.at(GlyphKey{ chosenSize, c });
                    advance = static_cast<double>(glyph->width);
                }
                if (c != L' ' && !glyph) missingGlyph = true;
                if (!characters.empty()) lineWidth += spacing;
                lineWidth += advance;
                characters.push_back({ glyph, advance });
            }
            // Do not accidentally mix Small/Medium/Large to fill absent glyphs.
            // Caller keeps its existing whole-name fallback if any glyph is missing.
            if (missingGlyph || lineWidth <= 0.0) return false;
            const double scale = (std::min)(1.0,
                (std::min)(static_cast<double>(box.width) / lineWidth,
                    static_cast<double>(box.height) / chosenSize));
            if (scale <= 0.0) return false;
            const int scaledWidth = (std::min)(box.width,
                static_cast<int>(std::lround(lineWidth * scale)));
            const int scaledHeight = (std::max)(1,
                static_cast<int>(std::lround(chosenSize * scale)));
            const int startX = box.x + (box.width - scaledWidth) / 2;
            const int startY = box.y + (box.height - scaledHeight) / 2;
            const int endX = box.x + box.width;
            const int endY = box.y + box.height;
            double offset = 0.0;
            for (const Placed& character : characters)
            {
                const int destX = startX + static_cast<int>(std::lround(offset));
                offset += character.advance * scale;
                const int destEnd = startX + static_cast<int>(std::lround(offset));
                offset += spacing * scale;
                if (!character.glyph) continue;
                const PlayerGlyph& glyph = *character.glyph;
                const int destW = (std::max)(1, destEnd - destX);
                for (int y = 0; y < scaledHeight; ++y)
                {
                    const int py = startY + y;
                    if (py < box.y || py >= endY || py < 0 || py >= int(HEIGHT)) continue;
                    const int sy = (std::min)(glyph.height - 1,
                        static_cast<int>(std::int64_t(y) * glyph.height / scaledHeight));
                    for (int x = 0; x < destW; ++x)
                    {
                        const int px = destX + x;
                        if (px < box.x || px >= endX || px < 0 || px >= int(WIDTH)) continue;
                        const int sx = (std::min)(glyph.width - 1,
                            static_cast<int>(std::int64_t(x) * glyph.width / destW));
                        const BYTE* src = glyph.pixels.data() +
                            (std::size_t(sy) * glyph.width + sx) * 4;
                        BYTE* dst = canvas + (std::size_t(py) * WIDTH + px) * 4;
                        const unsigned alpha = src[3];
                        if (!alpha) continue;
                        if (alpha == 255) std::memcpy(dst, src, 4);
                        else
                        {
                            const unsigned inverse = 255 - alpha;
                            for (int channel = 0; channel < 4; ++channel)
                                dst[channel] = static_cast<BYTE>((std::min)(255u,
                                    unsigned(src[channel]) +
                                    (unsigned(dst[channel]) * inverse + 127) / 255));
                        }
                    }
                }
            }
            return true;
        }

        // Main-button PNG hit test: use the existing cached alpha, never a rectangle.
        // Called only on a mouse click; no animation or preload work added.
        bool MainButtonPixel(const std::wstring& path, std::uint32_t x, std::uint32_t y)
        {
            if (path.empty() || x >= WIDTH || y >= HEIGHT) return false;
            FILE* file = nullptr;
            if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
            const long long offset =
                (static_cast<long long>(y) * WIDTH + x) * 4 + 3;
            const bool hit = _fseeki64(file, offset, SEEK_SET) == 0 &&
                fgetc(file) > 0;
            fclose(file);
            return hit;
        }

        // The category PNGs already have their final dimensions and canvas position.
        // For a selection, the current base Toth bitmap is read from cache again;
        // therefore the previous selection is removed automatically (- old), then
        // the new selected PNG is added (+ new), without offsets, crop or scaling.
        // This runs only when Toth's revision changes, never during Prime animation.
        bool AddSelectedIcon(BYTE* canvas, const std::wstring& cachePath)
        {
            if (!canvas || cachePath.empty()) return false;
            std::ifstream source(cachePath, std::ios::binary);
            if (!source) return false;

            std::vector<BYTE> row(std::size_t(WIDTH) * 4);
            for (UINT y = 0; y < HEIGHT; ++y)
            {
                if (!source.read(reinterpret_cast<char*>(row.data()),
                    static_cast<std::streamsize>(row.size())))
                    return false;
                BYTE* destination = canvas + std::size_t(y) * WIDTH * 4;
                for (UINT x = 0; x < WIDTH; ++x)
                {
                    const BYTE* pixel = row.data() + std::size_t(x) * 4;
                    BYTE* target = destination + std::size_t(x) * 4;
                    const unsigned alpha = pixel[3];
                    if (!alpha) continue;
                    if (alpha == 255)
                    {
                        std::memcpy(target, pixel, 4);
                        continue;
                    }
                    const unsigned remaining = 255 - alpha;
                    for (unsigned channel = 0; channel < 4; ++channel)
                    {
                        const unsigned value = unsigned(pixel[channel]) +
                            (unsigned(target[channel]) * remaining + 127) / 255;
                        target[channel] = static_cast<BYTE>((std::min)(255u, value));
                    }
                }
            }
            return true;
        }

        bool ValidCache(const std::wstring& path)
        {
            WIN32_FILE_ATTRIBUTE_DATA file{};
            return !path.empty() &&
                GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &file) &&
                !(file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                file.nFileSizeHigh == 0 && file.nFileSizeLow == BYTES;
        }

        bool AttachGame()
        {
            if (presentation) return true;
            if (!Cache.bitmap) return false;
            HDC dc = CreateCompatibleDC(nullptr);
            if (!dc) return false;
            HGDIOBJ original = SelectObject(dc, Cache.bitmap);
            if (!original || original == HGDI_ERROR)
            {
                DeleteDC(dc);
                return false;
            }
            presentation = dc;
            presentationOriginal = original;
            return true;
        }

        bool PrepareHarvestBack()
        {
            if (!Cache.game) return true;
            const auto* settlement =
                Cache.game->group.settlement.load(std::memory_order_acquire);
            if (!settlement || !settlement->ready) return true;

            const auto* back = settlement->steps[0].group.back;
            if (!back || !back->ready) return true;
            if (back->path == harvestBackPath) return true;
            if (!ValidCache(back->path)) return false;

            std::ifstream source(back->path, std::ios::binary);
            if (!source) return false;

            std::vector<BackPixel> pixels;
            std::vector<BYTE> row(std::size_t(WIDTH) * 4);
            for (UINT y = 0; y < HEIGHT; ++y)
            {
                if (!source.read(reinterpret_cast<char*>(row.data()),
                    static_cast<std::streamsize>(row.size()))) return false;

                for (UINT x = 0; x < WIDTH; ++x)
                {
                    const BYTE* pixel = row.data() + std::size_t(x) * 4;
                    if (!pixel[3]) continue;
                    BackPixel p{};
                    p.offset = static_cast<std::uint32_t>(
                        (std::size_t(y) * WIDTH + x) * 4);
                    std::memcpy(p.bgra, pixel, 4);
                    pixels.push_back(p);
                }
            }
            harvestBackPixels.swap(pixels);
            harvestBackPath = back->path;
            displayedInventoryRevision = UINT64_MAX;
            return true;
        }

        // +Back on the already selected Harvest cache, without a new DIB.
        void AddHarvestBack(BYTE* canvas)
        {
            for (const auto& pixel : harvestBackPixels)
            {
                BYTE* destination = canvas + pixel.offset;
                const unsigned alpha = pixel.bgra[3];
                if (alpha == 255)
                {
                    std::memcpy(destination, pixel.bgra, 4);
                    continue;
                }
                const unsigned remaining = 255 - alpha;
                for (unsigned c = 0; c < 4; ++c)
                {
                    const unsigned value = unsigned(pixel.bgra[c]) +
                        (unsigned(destination[c]) * remaining + 127) / 255;
                    destination[c] = static_cast<BYTE>((std::min)(255u, value));
                }
            }
        }

        // Reuse a Math bitmap cache. A changed bushel stage replaces the
        // sparse layer; no PNG decoding, extra DIB, or source Build() call.
        template<class Bitmap>
        bool PrepareBushelStage(const Bitmap* bitmap,
            std::wstring& currentPath, std::vector<BackPixel>& pixels)
        {
            const std::wstring path = bitmap && bitmap->ready ? bitmap->path : L"";
            if (currentPath == path) return true;
            if (path.empty())
            {
                currentPath.clear();
                pixels.clear();
                return true;
            }
            if (!ValidCache(path)) return false;
            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            std::vector<BackPixel> incoming;
            std::vector<BYTE> row(std::size_t(WIDTH) * 4);
            for (UINT y = 0; y < HEIGHT; ++y)
            {
                if (!file.read(reinterpret_cast<char*>(row.data()),
                    static_cast<std::streamsize>(row.size()))) return false;
                for (UINT x = 0; x < WIDTH; ++x)
                {
                    const BYTE* color = row.data() + std::size_t(x) * 4;
                    if (!color[3]) continue;
                    BackPixel pixel{};
                    pixel.offset = static_cast<std::uint32_t>(
                        (std::size_t(y) * WIDTH + x) * 4);
                    std::memcpy(pixel.bgra, color, 4);
                    incoming.push_back(pixel);
                }
            }
            pixels.swap(incoming);
            currentPath = path;
            return true;
        }

        void AddBushel(BYTE* canvas, const std::vector<BackPixel>& pixels)
        {
            for (const auto& pixel : pixels)
            {
                BYTE* target = canvas + pixel.offset;
                const UINT alpha = pixel.bgra[3];
                if (alpha == 255)
                {
                    std::memcpy(target, pixel.bgra, 4);
                    continue;
                }
                const UINT remaining = 255 - alpha;
                for (UINT c = 0; c < 4; ++c)
                {
                    const UINT value = UINT(pixel.bgra[c]) +
                        (UINT(target[c]) * remaining + 127) / 255;
                    target[c] = BYTE((std::min)(255u, value));
                }
            }
        }

        // A click and an idle swing have independent 1-damage sources.
        void ForwardBaseStats() {
            const auto* character = Cache.game ? Cache.game->group.character.load(std::memory_order_acquire) : nullptr;
            if (!character || !character->stats || !character->stats->ready)return;
            const auto& s = *character->stats;
            SAO_Progression_Conductor::ReceiveBase({ s.baseDamage,s.baseClickDamage,
                s.total.strength,s.total.dexterity,s.total.endurance,s.total.precision });
        }

        // Both consume the same forwarded Character stats and Harvest groups.
        // The active Character loop is never tied to mouse input.
        void CompleteHit(bool clickDamage)
        {
            if (!Cache.game || !harvestSelected || Cache.harvest.health == 0) return;
            const auto* character =
                Cache.game->group.character.load(std::memory_order_acquire);
            const auto* harvest =
                Cache.game->group.harvest.load(std::memory_order_acquire);
            if (!character || !character->stats || !harvest) return;

            ForwardBaseStats();
            const auto& stats = SAO_Progression_Conductor::Cache.stats;
            const double base = clickDamage ? stats.clickDamage : stats.damage;
            const UINT damageUnits = static_cast<UINT>((std::max)(1LL,
                std::llround((std::min)(base, 100.0) * SAO_Progression_Math::UNIT)));
            const UINT previousStage = Cache.harvest.removedBushels;
            const UINT applied = (std::min)(damageUnits, Cache.harvest.healthUnits);
            Cache.harvest.healthUnits -= applied;
            Cache.harvest.lastDamage = (applied + 199) / 200;
            Cache.harvest.health = (Cache.harvest.healthUnits + 199) / 200;
            ++Cache.harvest.completedHits;

            const UINT stage = (20000 - Cache.harvest.healthUnits) / 2000;
            for (UINT i = previousStage + 1; i <= stage; ++i)
            {
                const auto& old = harvest->hit[i - 1];
                const auto& next = harvest->hit[i];
                const auto rewards = SAO_Progression_Conductor::ReceiveHarvest(
                    2 * (next.grassReward - old.grassReward), next.strawReward - old.strawReward, false);
                Cache.harvest.grassItems += rewards.grass;
                Cache.harvest.strawItems += rewards.straw;
            }
            Cache.harvest.removedBushels = stage;
            if (Cache.harvest.healthUnits == 0) {
                SAO_Progression_Conductor::ReceiveHarvest(0, 0, true);
                restoreBushelsNextTick = true;
            }
        }

        bool PrepareCharacter()
        {
            if (!Cache.game) return true;
            const auto* character = Cache.game->group.character.load(std::memory_order_acquire);
            if (!character || !character->ready) return true;
            const UINT limit = character->complete ? SAO_Character_Harvest::COUNT : 1;
            std::vector<BYTE> row(std::size_t(WIDTH) * 4);
            for (UINT i = 0; i < limit; ++i)
            {
                const auto& step = character->steps[i];
                if (step.oakSign == SAO_Character_Harvest::Polarity::Plus && step.oakTool) {
                    if (!PrepareBushelStage(step.oakTool, oakToolPaths[i], oakToolPixels[i]))return false;
                }
                if (characterPrepared[i]) continue;
                const auto& frame = character->steps[i].frame;
                if (!frame.ready || !ValidCache(frame.path)) return false;
                std::ifstream source(frame.path, std::ios::binary);
                if (!source) return false;
                std::vector<BackPixel> result;
                for (UINT y = 0; y < HEIGHT; ++y)
                {
                    if (!source.read(reinterpret_cast<char*>(row.data()),
                        static_cast<std::streamsize>(row.size()))) return false;
                    for (UINT x = 0; x < WIDTH; ++x)
                    {
                        const BYTE* pixel = row.data() + std::size_t(x) * 4;
                        if (!pixel[3]) continue;
                        BackPixel visible{};
                        visible.offset = static_cast<std::uint32_t>(
                            (std::size_t(y) * WIDTH + x) * 4);
                        std::memcpy(visible.bgra, pixel, 4);
                        result.push_back(visible);
                    }
                }
                characterPixels[i].swap(result);
                characterPrepared[i] = true;
            }
            return true;
        }

        bool AddEquippedTool(BYTE* canvas, UINT pose) {
            const auto& inventory = SAO_Gear_Inventory_Conductor::Cache;
            if (inventory.toolSign == SAO_Gear_Inventory_Conductor::Polarity::Minus) {
                // +Oak -Stone: reuse the separately forwarded oak pose.
                AddBushel(canvas, oakToolPixels[pose % SAO_Character_Harvest::COUNT]);
                return true;
            }
            // -Oak +Stone: the body composite contains neither tool.
            if (!inventory.sickle || !inventory.sickle->ready)return false;
            static_assert(SAO_Stone_Sickle_Math::COUNT == SAO_Character_Harvest::COUNT,
                "Sickle poses must match Character poses.");
            const auto& step = inventory.sickle->steps[pose % SAO_Stone_Sickle_Math::COUNT];
            if (step.sign == SAO_Stone_Sickle_Conductor::Polarity::Minus)return true;
            const auto& group = step.group;
            if (group.width != WIDTH || group.height != HEIGHT) {
                Cache.error = L"Sickle pose canvas must match Character canvas; no scaling/padding of poses.";
                return false;
            }
            for (const auto& p : group.pixels) {
                BYTE* dest = canvas + (std::size_t(p.y) * WIDTH + p.x) * 4;
                const UINT inverse = 255 - p.bgra[3];
                for (UINT c = 0; c < 4; ++c)
                    dest[c] = BYTE((std::min)(255u, UINT(p.bgra[c]) + (UINT(dest[c]) * inverse + 127) / 255));
            }
            return true;
        }

        void AddCharacter(BYTE* canvas, UINT gameFrame)
        {
            const UINT index = gameFrame % SAO_Character_Harvest::COUNT;
            if (!characterPrepared[index]) return;
            for (const auto& visible : characterPixels[index])
            {
                BYTE* target = canvas + visible.offset;
                const UINT alpha = visible.bgra[3];
                const UINT inverse = 255 - alpha;
                for (UINT c = 0; c < 4; ++c)
                {
                    const UINT value = UINT(visible.bgra[c]) +
                        (UINT(target[c]) * inverse + 127) / 255;
                    target[c] = static_cast<BYTE>((std::min)(255u, value));
                }
            }
        }

        bool Restore(UINT frame)
        {
            if (!Cache.game || !Cache.pixels || frame >= FRAMES) return false;
            if (harvestSelected)
            {
                const auto* harvest =
                    Cache.game->group.harvest.load(std::memory_order_acquire);
                if (!harvest || !harvest->ready) return false;
                const auto& step =
                    harvest->steps[frame % SAO_Harvest_Tall_Grass_Math::COUNT];
                const auto* scene = step.group.scene;
                if (!scene || !scene->ready || !GdiFlush() ||
                    !Read(scene->path, Cache.pixels)) return false;

                // One fully grouped Stage cache from Harvest Conductor.
                // The normal 33-ms path does NOT reopen or regroup ten layers.
                const auto& stage = harvest->hit[Cache.harvest.removedBushels];
                if (!stage.overlay.ready ||
                    !PrepareBushelStage(&stage.overlay, bushelStagePath, bushelStagePixels))
                    return false;
                AddBushel(Cache.pixels, bushelStagePixels);
                if (!PrepareHarvestBack()) return false;
                // Preserve the accepted body order, then add exactly one Tool.
                AddCharacter(Cache.pixels, actionFrame);
                if (!AddEquippedTool(Cache.pixels, actionFrame))return false;
                AddHarvestBack(Cache.pixels);
                return true;
            }
            if (settlementSelected)
            {
                const auto* settlement = Cache.game->group.settlement.load(std::memory_order_acquire);
                if (!settlement || !settlement->ready) return false;
                const auto& step = settlement->steps[frame % SAO_Settlement_Math::COUNT];
                return step.frame.ready && GdiFlush() && Read(step.frame.path, Cache.pixels);
            }
            const auto& step = Cache.game->steps[frame];
            return step.frame.ready && GdiFlush() && Read(step.frame.path, Cache.pixels);
        }

        // -Settlement +Main: select an already-forwarded cache, no source calls.
        bool ReturnMain()
        {
            if (!settlementSelected) return true;
            if (!Cache.game || !Cache.game->ready ||
                !Cache.game->steps[0].frame.ready) return false;
            const UINT previous = Cache.frame;
            settlementSelected = false;
            startPending = false;
            harvestPending = false;
            Cache.frame = 0;
            if (!Restore(0))
            {
                settlementSelected = true;
                Cache.frame = previous;
                return false;
            }
            Cache.animation_ready = Cache.game->animation_ready &&
                Cache.game->complete.load(std::memory_order_acquire);
            Cache.preload_error = Cache.game->preload_error;
            return true;
        }

        // An existing Harvest cache is selected at this final projection node.
        // The source nodes never receive screen selection callbacks.
        bool SelectHarvest()
        {
            startPending = false;
            if (harvestSelected) return true;
            const auto* harvest = Cache.game ?
                Cache.game->group.harvest.load(std::memory_order_acquire) : nullptr;
            if (!harvest || !harvest->ready)
            {
                harvestPending = true;
                return true;
            }
            const UINT previousFrame = Cache.frame;
            const bool previousSettlement = settlementSelected;
            harvestSelected = true;
            settlementSelected = false;
            Cache.frame = 0;
            actionFrame = 0;
            restoreBushelsNextTick = false;
            Cache.harvest.swinging = true;
            if (!Restore(0))
            {
                harvestSelected = false;
                settlementSelected = previousSettlement;
                Cache.frame = previousFrame;
                return false;
            }
            harvestPending = false;
            const auto* character =
                Cache.game->group.character.load(std::memory_order_acquire);
            Cache.animation_ready = Cache.game->animation_ready &&
                Cache.game->complete.load(std::memory_order_acquire) &&
                harvest->complete.load(std::memory_order_acquire) && harvest->animation_ready &&
                character && character->complete &&
                characterPrepared[SAO_Character_Harvest::COUNT - 1];
            Cache.preload_error = harvest->preload_error;
            return true;
        }

        bool SelectSettlement()
        {
            harvestPending = false;
            actionFrame = 0;
            Cache.harvest.swinging = false;
            if (settlementSelected && !harvestSelected) return true;
            const auto* settlement = Cache.game ?
                Cache.game->group.settlement.load(std::memory_order_acquire) : nullptr;
            if (!settlement || !settlement->ready)
            {
                startPending = true;
                return true;
            }
            const UINT previousFrame = Cache.frame;
            const bool previousHarvest = harvestSelected;
            settlementSelected = true;
            harvestSelected = false;
            Cache.frame = 0;
            if (!Restore(0))
            {
                settlementSelected = false;
                harvestSelected = previousHarvest;
                Cache.frame = previousFrame;
                return false;
            }
            startPending = false;
            Cache.animation_ready = Cache.game->animation_ready &&
                Cache.game->complete.load(std::memory_order_acquire) &&
                settlement->complete.load(std::memory_order_acquire) && settlement->animation_ready;
            Cache.preload_error = settlement->preload_error;
            return true;
        }

        bool ReturnPrevious()
        {
            if (harvestSelected) return SelectSettlement(); // -Harvest +Settlement
            return ReturnMain();                           // -Settlement +Main
        }

        bool Forward()
        {
            ForwardBaseStats();
            SAO_Progression_Conductor::Save();
            const auto* itemSource = Cache.game ?
                Cache.game->group.harvest.load(std::memory_order_acquire) : nullptr;
            if (itemSource && itemSource->ready) {
                const auto* grass = itemSource->grassItem;
                const auto* straw = itemSource->strawItem;
                if (grass && grass->ready)
                    SAO_Gear_Inventory_Conductor::ReceiveItemArt(false, grass->path, grass->width, grass->height);
                if (straw && straw->ready)
                    SAO_Gear_Inventory_Conductor::ReceiveItemArt(true, straw->path, straw->width, straw->height);
            }
            // A prior harvest group has already forwarded completed numeric rewards.
            // Changes to the inventory are event-driven and saved, never per-frame.
            SAO_Gear_Inventory_Conductor::ReceiveHarvest(
                Cache.harvest.grassItems, Cache.harvest.strawItems);
            return destination && PostMessageW(destination, SAO_INPUT, 0,
                reinterpret_cast<LPARAM>(&Cache)) != FALSE;
        }

        // Draw runtime English inside the *existing Location PNG* bounds.
        // This executes only when Toth's revision changes. Never on Prime ticks.
        void DrawWords(HDC dc, BYTE* pixels, const SAO_Toth::TextLocation& loc,
            const std::wstring& text, COLORREF color, bool singleLine = false)
        {
            if (!loc.ready || text.empty() || !dc || !pixels) return;
            const int left = (std::max)(0, loc.x);
            const int top = (std::max)(0, loc.y);
            const int right = (std::min)(int(WIDTH), loc.x + loc.width);
            const int bottom = (std::min)(int(HEIGHT), loc.y + loc.height);
            if (right <= left || bottom <= top) return;
            const std::size_t stride = std::size_t(right - left) * 4;
            const std::size_t count = std::size_t(bottom - top);
            std::vector<BYTE> before(stride * count);
            for (int y = top; y < bottom; ++y)
                memcpy(before.data() + std::size_t(y - top) * stride,
                    pixels + (std::size_t(y) * WIDTH + left) * 4, stride);

            // Fit the complete player name into its PNG-defined location.
            const int fontHeight = singleLine ?
                (std::max)(11, (std::min)(23,
                    loc.width * 16 / (std::max)(16, static_cast<int>(text.size()) * 10))) : 23;
            HFONT font = CreateFontW(-fontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            if (!font) return;
            HGDIOBJ oldFont = SelectObject(dc, font);
            const int oldBackground = SetBkMode(dc, TRANSPARENT);
            const COLORREF oldColor = SetTextColor(dc, color);
            RECT rectangle{ left, top, right, bottom };
            DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &rectangle,
                singleLine ? (DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX) :
                (DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX));
            GdiFlush();
            SetTextColor(dc, oldColor);
            SetBkMode(dc, oldBackground);
            SelectObject(dc, oldFont);
            DeleteObject(font);
            // GDI leaves the alpha channel undefined for glyph pixels. Detect
            // changed glyph RGB and give them opaque alpha in the layered DIB.
            for (int y = top; y < bottom; ++y)
            {
                const BYTE* saved = before.data() + std::size_t(y - top) * stride;
                BYTE* rendered = pixels + (std::size_t(y) * WIDTH + left) * 4;
                for (std::size_t x = 0; x < stride; x += 4)
                    if (rendered[x] != saved[x] || rendered[x + 1] != saved[x + 1] ||
                        rendered[x + 2] != saved[x + 2])
                        rendered[x + 3] = 255;
            }
        }

        // Present Toth once on the window thread, not on animation ticks.
        // The new static Inventory/Gear Maths have already bitmap-cached source PNGs.
        // Compose only on toggle or a drag/stack update; NEVER on animation ticks.
        void AddPanel(BYTE* canvas, const SAO_Static_PNG_Cache::BitmapNode* node,
            UINT xStart) {
            if (!canvas || !node || !node->ready || node->height != HEIGHT ||
                (node->width != WIDTH / 2 && node->width != WIDTH))return;
            FILE* file = nullptr;
            if (_wfopen_s(&file, node->path.c_str(), L"rb") != 0 || !file)return;
            std::vector<BYTE> row(std::size_t(node->width) * 4);
            for (UINT y = 0; y < node->height; ++y) {
                if (fread(row.data(), 1, row.size(), file) != row.size())break;
                for (UINT x = 0; x < node->width; ++x) {
                    const BYTE* src = row.data() + std::size_t(x) * 4;
                    if (!src[3])continue;
                    BYTE* dst = canvas + (std::size_t(y) * WIDTH + x +
                        (node->width == WIDTH / 2 ? xStart : 0)) * 4;
                    const UINT inv = 255 - src[3];
                    for (UINT c = 0; c < 3; ++c)
                        dst[c] = static_cast<BYTE>((std::min)(255u,
                            UINT(src[c]) + (UINT(dst[c]) * inv + 127) / 255));
                    dst[3] = static_cast<BYTE>((std::min)(255u,
                        UINT(src[3]) + (UINT(dst[3]) * inv + 127) / 255));
                }
            }
            fclose(file);
        }
        void DrawInventoryItems(HDC dc, BYTE* pixels) {
            using namespace SAO_Gear_Inventory_Conductor;
            const auto& inventory = SAO_Gear_Inventory_Conductor::Cache;
            if (!pixels || !inventory.ready || !inventory.panelsVisible || !inventory.inventory)return;
            auto draw = [&](const Item& item, int x, int y, const ItemArt* equipmentArt) {
                const ItemArt* art = item.kind == Kind::Grass ? &inventory.grassArt :
                    item.kind == Kind::Straw ? &inventory.strawArt :
                    item.kind == Kind::Equipment && item.equipmentId == SAO_Stone_Sickle_Conductor::ITEM_ID ? equipmentArt : nullptr;
                if (item.kind == Kind::Equipment && !art) {
                    SAO_Toth::TextLocation box{};
                    box.x = x - 42; box.y = y - 19; box.width = 84; box.height = 42; box.ready = true;
                    DrawWords(dc, pixels, box, L"Gear " + std::to_wstring(item.equipmentId), RGB(0, 0, 0), true);
                    return;
                }
                if (!art || art->pixels.empty())return;
                const int left = x - art->width / 2, top = y - art->height / 2;
                for (const auto& p : art->pixels) {
                    const int dx = left + p.x, dy = top + p.y;
                    if (dx < 0 || dy < 0 || dx >= int(WIDTH) || dy >= int(HEIGHT))continue;
                    BYTE* dest = pixels + (std::size_t(dy) * WIDTH + dx) * 4;
                    const UINT inv = 255 - p.bgra[3];
                    for (UINT c = 0; c < 4; ++c)
                        dest[c] = BYTE((std::min)(255u, UINT(p.bgra[c]) + (UINT(dest[c]) * inv + 127) / 255));
                }
                SAO_Toth::TextLocation box{};
                box.x = left; box.y = top + art->height; box.width = (std::max)(art->width, 60);
                box.height = 24; box.ready = true;
                DrawWords(dc, pixels, box, std::to_wstring(item.amount), RGB(0, 0, 0), true);
            };
            for (unsigned i = 0; i < inventory.items.size(); ++i) {
                if (inventory.drag.active && !inventory.drag.fromGear && inventory.drag.source == i)continue;
                const auto& a = inventory.inventory->group.anchors[i];
                if (a.ready)draw(inventory.items[i], a.x +
                    (inventory.inventory->group.locations[i]->width == WIDTH / 2 ? 960 : 0), a.y, &inventory.sickleInventoryArt[i]);
            }
            for (unsigned i = 0; i < inventory.equipment.size(); ++i) {
                if (inventory.drag.active && inventory.drag.fromGear && inventory.drag.source == i)continue;
                const auto& a = inventory.gear->group.anchors[i];
                if (a.ready)draw(inventory.equipment[i], a.x, a.y, &inventory.sickleToolArt);
            }
            if (inventory.drag.active) {
                const auto& moving = inventory.drag.fromGear ?
                    inventory.equipment[inventory.drag.source] : inventory.items[inventory.drag.source];
                draw(moving, inventory.drag.x, inventory.drag.y, inventory.drag.fromGear ?
                    &inventory.sickleToolArt : &inventory.sickleInventoryArt[inventory.drag.source]);
            }
        }
        bool ShowStaticToth()
        {
            const auto& progress = SAO_Progression_Conductor::Cache;
            if (staticShown && displayedRevision == tothState.revision &&
                (!tothState.progressSelected || displayedProgressRevision == progress.revision) &&
                displayedInventoryRevision == SAO_Gear_Inventory_Conductor::Cache.revision)
                return true;
            if (!destination || !tothState.ready || !ValidCache(tothState.overlayPath))
                return false;

            if (!staticToth)
            {
                // Owned POPUP, not a child: its transparent image stays above
                // the game without being part of Prime's frame bitmap.
                staticToth = CreateWindowExW(
                    WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                    L"STATIC", L"", WS_POPUP,
                    0, 0, static_cast<int>(WIDTH), static_cast<int>(HEIGHT),
                    destination, nullptr, GetModuleHandleW(nullptr), nullptr);
                if (!staticToth) return false;
            }

            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = static_cast<LONG>(WIDTH);
            info.bmiHeader.biHeight = -static_cast<LONG>(HEIGHT);
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;

            void* pixels = nullptr;
            HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS,
                &pixels, nullptr, 0);
            if (!bitmap || !pixels)
            {
                if (bitmap) DeleteObject(bitmap);
                return false;
            }
            bool ok = Read(tothState.overlayPath, static_cast<BYTE*>(pixels));
            if (ok)
            {
                HDC source = CreateCompatibleDC(nullptr);
                if (!source) ok = false;
                else
                {
                    HGDIOBJ original = SelectObject(source, bitmap);
                    if (!original || original == HGDI_ERROR) ok = false;
                    else
                    {
                        // The overlay PNG remains untouched in its own cache.
                        DrawWords(source, static_cast<BYTE*>(pixels),
                            tothState.view == SAO_Toth_Overlay_Conductor::View::Minimized ?
                            tothState.minimizedInputLocation : tothState.inputLocation,
                            tothState.inputText, RGB(247, 224, 168));
                        if (tothState.view == SAO_Toth_Overlay_Conductor::View::Maximized)
                        {
                            DrawWords(source, static_cast<BYTE*>(pixels),
                                tothState.tothOutputLocation, tothState.progressSelected ? progress.characterText : tothState.tothText,
                                RGB(252, 228, 176));
                            DrawWords(source, static_cast<BYTE*>(pixels),
                                tothState.bastetOutputLocation, tothState.progressSelected ? progress.harvestingText : tothState.bastetText,
                                RGB(200, 229, 245));
                            // Exactly as supplied: - old selection + selected cached PNG.
                            // The base was re-read above, so no previous selected layer remains.
                            AddSelectedIcon(static_cast<BYTE*>(pixels),
                                tothState.selectedResearchIcon);
                            AddSelectedIcon(static_cast<BYTE*>(pixels),
                                tothState.selectedPlayerIcon);
                            // One static name update per profile/view revision.
                            // Prefer SAO cached letter bitmaps. If they are not
                            // ready yet, still show the complete player name.
                            if (!DrawPlayerNameGlyphs(static_cast<BYTE*>(pixels),
                                tothState.playerNameLocation, tothState.playerName))
                            {
                                DrawWords(source, static_cast<BYTE*>(pixels),
                                    tothState.playerNameLocation, tothState.playerName,
                                    RGB(252, 228, 176), true);
                            }
                        }
                        const auto& bag = SAO_Gear_Inventory_Conductor::Cache;
                        if (bag.ready && bag.panelsVisible) {
                            // Explicit +Gear left, +Inventory right; preserves their
                            // native 960x1080 dimensions, no scale or offsets.
                            AddPanel(static_cast<BYTE*>(pixels), bag.gear->group.window, 0);
                            AddPanel(static_cast<BYTE*>(pixels), bag.inventory->group.window, 960);
                            DrawInventoryItems(source, static_cast<BYTE*>(pixels));
                            if (!PrepareHarvestBack())ok = false;
                            else AddHarvestBack(static_cast<BYTE*>(pixels));
                        }
                        SIZE size{ static_cast<LONG>(WIDTH), static_cast<LONG>(HEIGHT) };
                        POINT origin{ 0, 0 };
                        POINT screenOrigin{ 0, 0 };
                        if (!ClientToScreen(destination, &screenOrigin)) ok = false;
                        BLENDFUNCTION blend{};
                        blend.BlendOp = AC_SRC_OVER;
                        blend.SourceConstantAlpha = 255;
                        blend.AlphaFormat = AC_SRC_ALPHA;
                        if (ok)
                            ok = UpdateLayeredWindow(staticToth, nullptr, &screenOrigin,
                                &size, source, &origin, 0, &blend, ULW_ALPHA) != FALSE;
                        SelectObject(source, original);
                    }
                    DeleteDC(source);
                }
            }
            DeleteObject(bitmap);
            if (!ok) return false;

            ShowWindow(staticToth, SW_SHOWNOACTIVATE);
            displayedRevision = tothState.revision;
            displayedInventoryRevision = SAO_Gear_Inventory_Conductor::Cache.revision;
            displayedProgressRevision = progress.revision;
            staticShown = true;
            return true;
        }

        // Prime and Toth meet here, but keep independent cache ownership.
        // The static Toth image is NOT written into the moving Prime bitmap.
        bool Combine()
        {
            if (!Cache.game || !tothReceived) return true;
            if (!Cache.game->ready || !tothState.ready)
            {
                Cache.error = !Cache.game->ready ? Cache.game->error : tothState.error;
                Cache.ready = false;
                return Forward();
            }
            Cache.bitmap = Cache.game->bitmap;
            Cache.pixels = Cache.game->pixels;
            if (!Cache.bitmap || !Cache.pixels || !AttachGame() ||
                !ValidCache(tothState.overlayPath))
            {
                Cache.error = L"Prime game or Toth static overlay cache is missing.";
                Cache.ready = false;
                return Forward();
            }
            Cache.ready = true;
            if (!PrepareHarvestBack())return false;
            // Character composites are prepared when their final cache is forwarded,
            // before the animation is permitted to advance. No 23-frame cache
            // preparation inside the 33-ms draw sequence.
            const auto* preparedCharacter =
                Cache.game->group.character.load(std::memory_order_acquire);
            if (preparedCharacter && preparedCharacter->complete &&
                !PrepareCharacter())
            {
                Cache.error = L"Preloaded Character cache cannot be read.";
                Cache.ready = false;
                return Forward();
            }
            if (harvestSelected && !Restore(Cache.frame))
            {
                Cache.error = L"The forwarded Harvest/Character cache could not be restored.";
                Cache.ready = false;
                return Forward();
            }
            if (startPending)
            {
                const auto* settlement = Cache.game->group.settlement.load(std::memory_order_acquire);
                if (settlement && settlement->ready && !SelectSettlement())
                {
                    Cache.error = L"Settlement first projection could not be read.";
                    Cache.ready = false;
                    return Forward();
                }
            }
            if (harvestPending)
            {
                const auto* harvest = Cache.game->group.harvest.load(std::memory_order_acquire);
                if (harvest && harvest->ready && !SelectHarvest())
                {
                    Cache.error = L"Harvest first projection could not be read.";
                    Cache.ready = false;
                    return Forward();
                }
            }
            Cache.overlayRevision = tothState.revision;
            Cache.error.clear();

            // Toth's separate preload is tracked independently of Prime.
            const bool viewsReady = tothState.validated &&
                ValidCache(tothState.maximizedPath) &&
                ValidCache(tothState.minimizedPath);
            // Animation belongs to Prime. Toth validation must not disable
            // or stop the 33 ms game timer while its static cache is prepared.
            Cache.animation_ready = Cache.game->animation_ready &&
                Cache.game->complete.load(std::memory_order_acquire);
            Cache.preload_finished = tothState.validated;
            Cache.preload_error = tothState.validated && !viewsReady ?
                L"Toth static overlay cache preload was incomplete." : L"";
            if (harvestSelected)
            {
                const auto* harvest = Cache.game->group.harvest.load(std::memory_order_acquire);
                const bool completed = harvest &&
                    harvest->complete.load(std::memory_order_acquire);
                const auto* character =
                    Cache.game->group.character.load(std::memory_order_acquire);
                const bool characterReady = character && character->ready &&
                    character->complete &&
                    characterPrepared[SAO_Character_Harvest::COUNT - 1];
                Cache.animation_ready = Cache.animation_ready && completed &&
                    harvest->animation_ready && characterReady;
                if (completed && !harvest->animation_ready)
                    Cache.preload_error = harvest->preload_error;
            }
            if (settlementSelected)
            {
                const auto* settlement = Cache.game->group.settlement.load(std::memory_order_acquire);
                const bool completed = settlement &&
                    settlement->complete.load(std::memory_order_acquire);
                Cache.animation_ready = Cache.animation_ready && completed &&
                    settlement->animation_ready;
                if (completed && !settlement->animation_ready)
                    Cache.preload_error = settlement->preload_error;
            }
            return Forward();
        }

        // One-time migration from the old build that wrote Toth directly
        // into the 360 Prime frame files. Never repeat if no marker exists.
        bool UndoOldCombinedFiles()
        {
            wchar_t exe[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
            if (!n || n >= MAX_PATH) return false;
            std::wstring base(exe, n);
            base.resize(base.find_last_of(L"\\/") + 1);
            base += L"Cache\\SAO_Main_Screen_Math\\";
            const std::wstring marker = base + L"SAO_Main_Projection_Combined_State.bin";
            if (GetFileAttributesW(marker.c_str()) == INVALID_FILE_ATTRIBUTES)
                return true;
            for (UINT i = 0; i < FRAMES; ++i)
            {
                const std::wstring file = base + L"SAO_Main_Projection_v1_" +
                    std::to_wstring(i) + L".cache";
                if (!DeleteFileW(file.c_str()) && GetLastError() != ERROR_FILE_NOT_FOUND)
                    return false;
            }
            return DeleteFileW(marker.c_str()) != FALSE;
        }
    }

    bool Start(HWND program)
    {
        if (!UndoOldCombinedFiles()) return false;
        {
            std::lock_guard<std::mutex> lock(gate);
            destination = program;
        }
        return SAO_Toth::Start(program);
    }

    bool Build(const GameNode& game, HWND program)
    {
        bool notifyToth = false;
        bool success = false;
        {
            std::lock_guard<std::mutex> lock(gate);
            destination = program;
            if (Cache.game && Cache.game != &game)
            {
                Cache.error = L"Unexpected replacement of Prime's game cache.";
                Cache.ready = false;
                return Forward();
            }
            Cache.game = &game;
            success = Combine();
            if (preloadRequested && !tothPreloadIssued &&
                game.complete.load(std::memory_order_acquire) && game.animation_ready)
            {
                tothPreloadIssued = true;
                notifyToth = true;
            }
        }
        if (notifyToth) SAO_Toth::Preload();
        return success;
    }

    bool ReceiveToth(const SAO_Toth::OutputNode& input, HWND program)
    {
        bool closeRequested = false;
        bool forwarded = false;
        {
            std::lock_guard<std::mutex> lock(gate);
            destination = program;
            tothState = input;
            tothReceived = true;
            if (!tothState.playerName.empty()) {
                const bool changed = SAO_Progression_Conductor::Cache.player != tothState.playerName;
                SAO_Progression_Conductor::SetProfile(tothState.playerName);
                if (changed) { Cache.harvest = {}; restoreBushelsNextTick = false; }
            }
            if (SAO_Gear_Inventory_Conductor::Cache.ready && !tothState.playerName.empty())
                SAO_Gear_Inventory_Conductor::SetProfile(tothState.playerName);
            if (input.progressSelected) {
                if (SAO_Gear_Inventory_Conductor::Cache.panelsVisible)
                    SAO_Gear_Inventory_Conductor::CloseBoth();
                inventoryPending = false;
            }
            if (input.inventoryRequested || input.gearRequested) {
                inventoryPending = !SAO_Gear_Inventory_Conductor::OpenBoth();
            }
            closeRequested = input.exitRequested;
            if (input.backRequested) {
                if (SAO_Gear_Inventory_Conductor::Cache.panelsVisible || inventoryPending) {
                    inventoryPending = false; SAO_Gear_Inventory_Conductor::CloseBoth();
                }
                else if (!ReturnPrevious())return false;
            }
            if (input.startGameRequested && !SelectSettlement()) return false;
            if (input.harvestTallGrassRequested && !SelectHarvest()) return false;
            forwarded = Combine();
        }
        // Exit is a user command, not part of animation or preload.
        // Post outside the cache lock; Program owns actual window shutdown.
        if (closeRequested && program)
            return PostMessageW(program, WM_CLOSE, 0, 0) != FALSE;
        return forwarded;
    }

    bool ReceiveInventory(const SAO_Inventory_Conductor::OutputNode& inventory,
        const SAO_Gear_Conductor::OutputNode& gear,
        const SAO_Stone_Sickle_Conductor::OutputNode& sickle)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (!SAO_Gear_Inventory_Conductor::Build(inventory, gear, sickle)) {
            Cache.error = SAO_Gear_Inventory_Conductor::Cache.error; return false;
        }
        if (!tothState.playerName.empty() &&
            !SAO_Gear_Inventory_Conductor::SetProfile(tothState.playerName))return false;
        if (inventoryPending) { SAO_Gear_Inventory_Conductor::OpenBoth(); inventoryPending = false; }
        if (harvestSelected && !Restore(Cache.frame))return false;
        return Forward();
    }

    void Preload()
    {
        bool notifyToth = false;
        {
            std::lock_guard<std::mutex> lock(gate);
            if (preloadRequested) return;
            preloadRequested = true;
            if (Cache.game && Cache.game->complete.load(std::memory_order_acquire) &&
                Cache.game->animation_ready && !tothPreloadIssued)
            {
                tothPreloadIssued = true;
                notifyToth = true;
            }
        }
        if (notifyToth) SAO_Toth::Preload();
    }

    bool Advance()
    {
        std::lock_guard<std::mutex> lock(gate);
        if (!Cache.ready || !Cache.animation_ready || !Cache.game) return false;
        const UINT next = Cache.game->steps[Cache.frame].next;

        const auto previousHarvest = Cache.harvest;
        const auto previousProgress = SAO_Progression_Conductor::Cache.state;
        const UINT previousActionFrame = actionFrame;
        const bool previousRestore = restoreBushelsNextTick;
        bool forwardNumbers = false;

        if (harvestSelected)
        {
            if (restoreBushelsNextTick)
            {
                // At 0 HP all ten have disappeared; restore the same ten
                // cached layers for the next cycle. Rewards remain cumulative.
                Cache.harvest.health = Cache.harvest.maxHealth;
                Cache.harvest.healthUnits = 20000;
                Cache.harvest.removedBushels = 0;
                Cache.harvest.lastDamage = 0;
                restoreBushelsNextTick = false;
                forwardNumbers = true;
            }
            actionFrame = (actionFrame + 1) % SAO_Character_Harvest::COUNT;
            Cache.harvest.swinging = true;
            if (actionFrame == 0)
            {
                // One idle damage at the end of every 23-pose swing.
                CompleteHit(false);
                forwardNumbers = true;
            }
        }
        if (!Restore(next))
        {
            Cache.harvest = previousHarvest;
            SAO_Progression_Conductor::Rollback(previousProgress);
            actionFrame = previousActionFrame;
            restoreBushelsNextTick = previousRestore;
            return false;
        }
        Cache.frame = next;
        if (forwardNumbers) return Forward();
        return true;
    }

    bool Present(HDC destinationDC)
    {
        // Called from Program's WM_PAINT on the window thread.
        std::lock_guard<std::mutex> lock(gate);
        if (!Cache.ready || !presentation || !destinationDC) return false;
        if (!BitBlt(destinationDC, 0, 0, WIDTH, HEIGHT,
            presentation, 0, 0, SRCCOPY)) return false;
        // Never suppress Program's two-second preload just because the
        // independent Toth display hasn't been presented yet.
        ShowStaticToth();
        return true;
    }

    bool Click(std::uint32_t x, std::uint32_t y, HWND program)
    {
        {
            std::lock_guard<std::mutex> lock(gate);
            if (SAO_Gear_Inventory_Conductor::Cache.panelsVisible) {
                if (!PrepareHarvestBack())return false;
                if (MainButtonPixel(harvestBackPath, x, y)) {
                    SAO_Gear_Inventory_Conductor::CloseBoth(); inventoryPending = false;
                    return Forward();
                }
            }
            if (SAO_Gear_Inventory_Conductor::Cache.panelsVisible &&
                SAO_Gear_Inventory_Conductor::MouseDown(int(x), int(y)))
            {
                // Compositor is invalidated, but no source image is re-decoded.
                return Forward();
            }
            if (SAO_Gear_Inventory_Conductor::Cache.panelsVisible)
                return true; // Covered game is not harvesting through the panel.
        }
        std::wstring exitButton, startButton, backButton, harvestingButton;
        std::array<std::wstring, 5> grassTargets{}, strawTargets{};
        {
            std::lock_guard<std::mutex> lock(gate);
            if (!Cache.ready) return false;
            if ((settlementSelected || harvestSelected) && Cache.game)
            {
                const auto* settlement =
                    Cache.game->group.settlement.load(std::memory_order_acquire);
                if (settlement && settlement->ready)
                {
                    const auto& group = settlement->steps[0].group;
                    if (group.back && group.back->ready) backButton = group.back->path;
                    if (settlementSelected && group.harvesting && group.harvesting->ready)
                        harvestingButton = group.harvesting->path;
                }
            }
            if (harvestSelected && Cache.harvest.health && Cache.game)
            {
                const auto* harvest =
                    Cache.game->group.harvest.load(std::memory_order_acquire);
                if (harvest && harvest->ready)
                {
                    const auto& stage = harvest->hit[Cache.harvest.removedBushels];
                    for (UINT piece = 0; piece < 5; ++piece)
                    {
                        const auto* grass = stage.grassSigns[piece] ==
                            SAO_Harvest_Tall_Grass_Conductor::Polarity::Plus ?
                            stage.grassLayers[piece] : nullptr;
                        const auto* straw = stage.strawSigns[piece] ==
                            SAO_Harvest_Tall_Grass_Conductor::Polarity::Plus ?
                            stage.strawLayers[piece] : nullptr;
                        if (grass && grass->ready) grassTargets[piece] = grass->path;
                        if (straw && straw->ready) strawTargets[piece] = straw->path;
                    }
                }
            }
            if (!settlementSelected && !harvestSelected && Cache.game &&
                Cache.game->input && Cache.game->input->buttons)
            {
                for (const auto& button : Cache.game->input->buttons->layers)
                {
                    if (!button.ready) continue;
                    if (button.name == L"Exit_Game.png") exitButton = button.path;
                    if (button.name == L"Start_Game.png") startButton = button.path;
                }
            }
        }
        // Existing cached PNG alpha only; no new click rectangles.
        if (MainButtonPixel(backButton, x, y))
        {
            std::lock_guard<std::mutex> lock(gate);
            if (!ReturnPrevious()) return false;
            return Forward();
        }
        if (MainButtonPixel(harvestingButton, x, y))
        {
            std::lock_guard<std::mutex> lock(gate);
            if (!SelectHarvest()) return false;
            return Forward();
        }
        if (MainButtonPixel(exitButton, x, y))
            return PostMessageW(program, WM_CLOSE, 0, 0) != FALSE;
        if (MainButtonPixel(startButton, x, y))
        {
            std::lock_guard<std::mutex> lock(gate);
            if (!SelectSettlement()) return false;
            return Forward();
        }
        bool clickedBushel = false;
        for (UINT piece = 0; piece < 5 && !clickedBushel; ++piece)
            clickedBushel = MainButtonPixel(grassTargets[piece], x, y) ||
            MainButtonPixel(strawTargets[piece], x, y);
        if (clickedBushel)
        {
            std::lock_guard<std::mutex> lock(gate);
            if (!harvestSelected || !Cache.harvest.health) return false;
            // Mouse damage is independent of the continuous Character swing.
            const auto before = Cache.harvest;
            const auto progressBefore = SAO_Progression_Conductor::Cache.state;
            const bool restoreBefore = restoreBushelsNextTick;
            CompleteHit(true);
            if (!Restore(Cache.frame))
            {
                Cache.harvest = before;
                SAO_Progression_Conductor::Rollback(progressBefore);
                restoreBushelsNextTick = restoreBefore;
                return false;
            }
            return Forward();
        }
        return SAO_Toth::Click(x, y, program);
    }

    bool MouseMove(std::uint32_t x, std::uint32_t y)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (!SAO_Gear_Inventory_Conductor::MouseMove(int(x), int(y)))return false;
        return Forward();
    }
    bool MouseUp(std::uint32_t x, std::uint32_t y)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (!SAO_Gear_Inventory_Conductor::MouseUp(int(x), int(y)))return false;
        if (harvestSelected && !Restore(Cache.frame))return false;
        return Forward();
    }
    bool RightClick(std::uint32_t x, std::uint32_t y)
    {
        std::lock_guard<std::mutex> lock(gate);
        if (!SAO_Gear_Inventory_Conductor::RightDelete(int(x), int(y)))return false;
        return Forward();
    }
    bool InventoryDragging() {
        std::lock_guard<std::mutex> lock(gate);
        return SAO_Gear_Inventory_Conductor::Cache.drag.active;
    }
    void Stop()
    {
        SAO_Toth::Stop();
        std::lock_guard<std::mutex> lock(gate);
        if (staticToth)
        {
            if (IsWindow(staticToth)) DestroyWindow(staticToth);
            staticToth = nullptr;
        }
        staticShown = false;
        displayedRevision = 0;
        displayedInventoryRevision = 0;
        SAO_Gear_Inventory_Conductor::Stop();
        SAO_Progression_Conductor::Save();
        if (presentation)
        {
            SelectObject(presentation, presentationOriginal);
            DeleteDC(presentation);
        }
        presentation = nullptr;
        presentationOriginal = nullptr;
        playerGlyphs.clear();
        harvestBackPixels.clear();
        harvestBackPath.clear();
        bushelStagePath.clear();
        bushelStagePixels.clear();
        actionFrame = 0;
        restoreBushelsNextTick = false;
        tothState = SAO_Toth::OutputNode{};
        tothReceived = preloadRequested = tothPreloadIssued = false;
        settlementSelected = harvestSelected = startPending = harvestPending = false;
        for (auto& pixels : characterPixels) pixels.clear();
        characterPrepared.fill(false);
        for (auto& pixels : oakToolPixels)pixels.clear();
        for (auto& path : oakToolPaths)path.clear();
        inventoryPending = false;
        Cache = OutputNode{};
        destination = nullptr;
    }
}








