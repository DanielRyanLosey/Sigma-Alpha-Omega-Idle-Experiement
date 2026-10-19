#include "SAO_Main_Screen_Conductor.h"
#include "SAO_Main_Screen_Conductor_Cache_Node.h"
#include <cstdio>
#include <vector>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace Main_Next_Node = SAO_Main_Screen_Conductor_Cache_Node;

namespace SAO_Main_Screen_Conductor
{
    OutputNode Cache{};
    namespace
    {
        constexpr UINT STRIDE = WIDTH * 4;
        const SAO_Bitmap_Cache_Node* window[4]{};
        const SAO_Bitmap_Cache_Node* symbols[3]{};
        const SAO_Bitmap_Cache_Node* scene[2]{};
        PairNode fractal[24], particle[45];
        std::wstring foreground;
        bool built = false;

        bool Stamp(const std::wstring& path, FILETIME& latest)
        {
            WIN32_FILE_ATTRIBUTE_DATA file{};
            if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &file) ||
                (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                file.nFileSizeHigh || file.nFileSizeLow != BYTES) return false;
            if (CompareFileTime(&file.ftLastWriteTime, &latest) > 0) latest = file.ftLastWriteTime;
            return true;
        }

        bool Fresh(const std::wstring& path, const FILETIME& source)
        {
            FILETIME saved{};
            return Stamp(path, saved) && CompareFileTime(&saved, &source) >= 0;
        }

        bool Read(const std::wstring& path, BYTE* pixels)
        {
            FILE* file = nullptr;
            if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
            const bool read = fread(pixels, 1, BYTES, file) == BYTES;
            return fclose(file) == 0 && read;
        }

        bool Save(const std::wstring& path, const BYTE* pixels)
        {
            const std::wstring temporary = path + L".tmp";
            FILE* file = nullptr;
            if (_wfopen_s(&file, temporary.c_str(), L"wb") != 0 || !file) return false;
            const bool written = fwrite(pixels, 1, BYTES, file) == BYTES;
            const bool closed = fclose(file) == 0;
            if (written && closed && MoveFileExW(temporary.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temporary.c_str());
            return false;
        }

        bool Project(const GroupNode& group, BYTE* pixels)
        {
            BYTE row[STRIDE];
            for (UINT layer = 0; layer < group.count; ++layer)
            {
                const auto* node = group.layers[layer];
                FILE* file = nullptr;
                if (!node || !node->ready || _wfopen_s(&file, node->path.c_str(), L"rb") != 0 || !file)
                    return false;
                bool success = true;
                for (UINT y = 0; y < HEIGHT && success; ++y)
                {
                    success = !SAO_Main_Screen_Math::Cancelled() && fread(row, 1, STRIDE, file) == STRIDE;
                    if (!success) break;
                    BYTE* front = pixels + y * STRIDE;
                    for (UINT x = 0; x < STRIDE; x += 4)
                    {
                        if (front[x + 3] == 255 || !row[x + 3]) continue;
                        const UINT remaining = 255 - front[x + 3];
                        for (UINT c = 0; c < 4; ++c)
                            front[x + c] = BYTE(front[x + c] + (row[x + c] * remaining + 127) / 255);
                    }
                }
                fclose(file);
                if (!success) return false;
            }
            return true;
        }

        bool Foreground(BYTE* pixels)
        {
            FILETIME latest{};
            for (const auto& group : { Cache.symbols, Cache.window })
                for (UINT i = 0; i < group.count; ++i)
                    if (!group.layers[i]->ready || !Stamp(group.layers[i]->path, latest)) return false;
            if (Fresh(foreground, latest)) return true;
            ZeroMemory(pixels, BYTES);
            return Project(Cache.symbols, pixels) && Project(Cache.window, pixels) && Save(foreground, pixels);
        }

        bool Frame(UINT index, BYTE* pixels, bool display)
        {
            auto& node = Cache.steps[index].frame;
            scene[0] = particle[index % 45].left;
            scene[1] = fractal[index % 24].left;
            FILETIME latest{};
            if (!Stamp(foreground, latest) || !scene[0]->ready || !scene[1]->ready ||
                !Stamp(scene[0]->path, latest) || !Stamp(scene[1]->path, latest)) return false;
            if (Fresh(node.path, latest))
                node.ready = !display || Read(node.path, pixels);
            else
                node.ready = Read(foreground, pixels) && Project(Cache.scene, pixels) && Save(node.path, pixels);
            return node.ready;
        }

        bool Initial(const SAO_Main_Screen_Math_Cache_Node::OutputNode& input)
        {
            Cache.input = &input;
            for (UINT i = 0; i < SAO_Main_Buttons_Math::COUNT; ++i) window[i] = &input.buttons->layers[i];
            window[3] = &input.screen->layers[24];
            symbols[0] = &input.title->layers[2];
            symbols[1] = &input.title->layers[0];
            symbols[2] = &input.title->layers[1];
            for (UINT i = 0; i < 24; ++i)
                fractal[i] = { &input.screen->layers[i], &input.screen->layers[(i + 1) % 24], (i + 1) % 24 };
            for (UINT i = 0; i < 45; ++i)
                particle[i] = { &input.particles->layers[i], &input.particles->layers[(i + 1) % 45], (i + 1) % 45 };
            Cache.fractal = &fractal[0]; Cache.particle = &particle[0];
            Cache.window = { window, 4 }; Cache.symbols = { symbols, 3 }; Cache.scene = { scene, 2 };
            const auto& path = window[3]->path;
            const auto folder = path.substr(0, path.find_last_of(L"\\/") + 1);
            foreground = folder + L"SAO_Main_Foreground.cache";
            for (UINT i = 0; i < FRAMES; ++i)
            {
                auto& step = Cache.steps[i];
                step.frame.name = L"SAO_Main_Projection_v1_" + std::to_wstring(i);
                step.frame.path = folder + step.frame.name + L".cache";
                step.next = (i + 1) % FRAMES;
            }
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = WIDTH; info.bmiHeader.biHeight = -LONG(HEIGHT);
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* pixels = nullptr;
            Cache.bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            Cache.pixels = static_cast<BYTE*>(pixels);
            return Cache.bitmap && Cache.pixels && Foreground(Cache.pixels) && Frame(0, Cache.pixels, true);
        }

        bool Prepare()
        {
            // Temporary preparation buffer only. The displayed bitmap is never touched here.
            std::vector<BYTE> pixels;
            for (UINT i = 0; i < FRAMES; ++i)
            {
                if (SAO_Main_Screen_Math::Cancelled()) return false;
                scene[0] = particle[i % 45].left; scene[1] = fractal[i % 24].left;
                FILETIME latest{};
                if (!Stamp(foreground, latest) || !scene[0]->ready || !scene[1]->ready ||
                    !Stamp(scene[0]->path, latest) || !Stamp(scene[1]->path, latest)) return false;
                if (Fresh(Cache.steps[i].frame.path, latest)) { Cache.steps[i].frame.ready = true; continue; }
                if (pixels.empty()) pixels.resize(BYTES);
                if (!Frame(i, pixels.data(), false)) return false;
            }
            return true;
        }
    }

    const OutputNode& Build(const SAO_Main_Screen_Math_Cache_Node::OutputNode& input, HWND target)
    {
        if (!built)
        {
            built = true;
            Cache.ready = input.ready && Initial(input);
            if (!Cache.ready) Cache.error = input.error.empty() ? L"The initial main projection could not be prepared." : input.error;
            if (!SAO_Main_Screen_Math::Cancelled()) Main_Next_Node::Build(Cache, target);
        }
        else if (!Cache.complete.load(std::memory_order_acquire))
        {
            Cache.animation_ready = input.ready && Prepare();
            if (!Cache.animation_ready)
                Cache.preload_error = input.error.empty() ? L"The animation projection caches could not be prepared. Check available disk space." : input.error;
            Cache.complete.store(true, std::memory_order_release);
            if (!SAO_Main_Screen_Math::Cancelled()) Main_Next_Node::Build(Cache, target);
        }
        return Cache;
    }
}