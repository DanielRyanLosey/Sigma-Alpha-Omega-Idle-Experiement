#include "SAO_Settlement_Conductor.h"
#include "SAO_Settlement_Conductor_Cache_Node.h"
#include <cstdio>

namespace SAO_Settlement_Conductor
{
    OutputNode Cache{};
    namespace
    {
        constexpr UINT WIDTH = 1920, HEIGHT = 1080;
        constexpr UINT STRIDE = WIDTH * 4, BYTES = STRIDE * HEIGHT;
        bool initialized = false;

        bool Directory(std::wstring& folder)
        {
            wchar_t executable[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
            if (!n || n >= MAX_PATH) return false;
            folder.assign(executable, n);
            folder.resize(folder.find_last_of(L"\\/") + 1);
            folder += L"Cache\\";
            if (!CreateDirectoryW(folder.c_str(), nullptr) &&
                GetLastError() != ERROR_ALREADY_EXISTS) return false;
            folder += L"SAO_Settlement_Conductor\\";
            return CreateDirectoryW(folder.c_str(), nullptr) ||
                GetLastError() == ERROR_ALREADY_EXISTS;
        }

        bool Stamp(const std::wstring& path, FILETIME& stamp)
        {
            WIN32_FILE_ATTRIBUTE_DATA item{};
            if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &item) ||
                (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                item.nFileSizeHigh || item.nFileSizeLow != BYTES) return false;
            if (CompareFileTime(&item.ftLastWriteTime, &stamp) > 0)
                stamp = item.ftLastWriteTime;
            return true;
        }

        bool Fresh(const std::wstring& path, const FILETIME& source)
        {
            FILETIME saved{};
            return Stamp(path, saved) && CompareFileTime(&saved, &source) >= 0;
        }

        bool Compose(const GroupNode& group, const std::wstring& path)
        {
            if (!group.scene || !group.harvesting || !group.back ||
                !group.scene->ready || !group.harvesting->ready ||
                !group.back->ready) return false;
            FILETIME stamp{};
            if (!Stamp(group.scene->path, stamp) ||
                !Stamp(group.harvesting->path, stamp) ||
                !Stamp(group.back->path, stamp)) return false;
            if (Fresh(path, stamp)) return true;

            FILE* scene = nullptr;
            FILE* overlay = nullptr;
            FILE* back = nullptr;
            FILE* output = nullptr;
            const std::wstring temp = path + L".tmp";
            bool ok = _wfopen_s(&scene, group.scene->path.c_str(), L"rb") == 0 && scene;
            if (ok) ok = _wfopen_s(&overlay, group.harvesting->path.c_str(), L"rb") == 0 && overlay;
            if (ok) ok = _wfopen_s(&back, group.back->path.c_str(), L"rb") == 0 && back;
            if (ok) ok = _wfopen_s(&output, temp.c_str(), L"wb") == 0 && output;
            BYTE base[STRIDE], top[STRIDE];
            for (UINT y = 0; y < HEIGHT && ok; ++y)
            {
                ok = fread(base, 1, STRIDE, scene) == STRIDE &&
                    fread(top, 1, STRIDE, overlay) == STRIDE;
                if (!ok) break;
                for (UINT i = 0; i < STRIDE; i += 4)
                {
                    const UINT a = top[i + 3];
                    if (!a) continue;
                    const UINT remain = 255 - a;
                    for (UINT c = 0; c < 4; ++c)
                        base[i + c] = BYTE(top[i + c] +
                            (UINT(base[i + c]) * remain + 127) / 255);
                }
                ok = fread(top, 1, STRIDE, back) == STRIDE;
                if (!ok) break;
                for (UINT i = 0; i < STRIDE; i += 4)
                {
                    const UINT a = top[i + 3];
                    if (!a) continue;
                    const UINT remain = 255 - a;
                    for (UINT c = 0; c < 4; ++c)
                        base[i + c] = BYTE(top[i + c] +
                            (UINT(base[i + c]) * remain + 127) / 255);
                }
                ok = fwrite(base, 1, STRIDE, output) == STRIDE;
            }
            if (scene && fclose(scene) != 0) ok = false;
            if (overlay && fclose(overlay) != 0) ok = false;
            if (back && fclose(back) != 0) ok = false;
            if (output && fclose(output) != 0) ok = false;
            if (ok && MoveFileExW(temp.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temp.c_str());
            return false;
        }

        bool Initialize(const SAO_Settlement_Math::OutputNode& input)
        {
            std::wstring folder;
            if (!Directory(folder)) return false;
            for (UINT i = 0; i < SAO_Settlement_Math::COUNT; ++i)
            {
                auto& step = Cache.steps[i];
                step.group.scene = &input.scenes[i];
                step.group.harvesting = &input.harvesting;
                step.group.back = &input.back;
                step.frame.name = L"SAO_Settlement_Projection_" + std::to_wstring(i);
                step.frame.path = folder + step.frame.name + L".cache";
                step.next = (i + 1) % SAO_Settlement_Math::COUNT;
            }
            return true;
        }

        bool Prepare(UINT first, UINT last)
        {
            for (UINT i = first; i < last; ++i)
            {
                auto& frame = Cache.steps[i];
                frame.frame.ready = Compose(frame.group, frame.frame.path);
                if (!frame.frame.ready) return false;
            }
            return true;
        }
    }

    const OutputNode& Build(const SAO_Settlement_Math::OutputNode& input, HWND target)
    {
        if (!initialized)
        {
            Cache.ready = input.ready && Initialize(input) && Prepare(0, 1);
            if (!Cache.ready)
            {
                Cache.error = input.error.empty() ?
                    L"Settlement's first conductor cache failed." : input.error;
                // Keep the branch uninitialized so the next forward cache can succeed.
                SAO_Settlement_Conductor_Cache_Node::Build(Cache, target);
                return Cache;
            }
            initialized = true;
            Cache.error.clear();
            SAO_Settlement_Conductor_Cache_Node::Build(Cache, target);
        }
        if (initialized && input.complete.load(std::memory_order_acquire) &&
            !Cache.complete.load(std::memory_order_acquire))
        {
            Cache.animation_ready = input.ready &&
                Prepare(1, SAO_Settlement_Math::COUNT);
            if (!Cache.animation_ready)
                Cache.preload_error = input.error.empty() ?
                L"Settlement's animated conductor caches failed." : input.error;
            else
                Cache.preload_error.clear();
            Cache.complete.store(true, std::memory_order_release);
            SAO_Settlement_Conductor_Cache_Node::Build(Cache, target);
        }
        return Cache;
    }
}