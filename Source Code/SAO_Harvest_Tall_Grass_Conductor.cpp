#include "SAO_Harvest_Tall_Grass_Conductor.h"
#include "SAO_Harvest_Tall_Grass_Conductor_Cache_Node.h"
#include <cstdio>

namespace SAO_Harvest_Tall_Grass_Conductor
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
            folder += L"SAO_Harvest_Tall_Grass_Conductor\\";
            return CreateDirectoryW(folder.c_str(), nullptr) ||
                GetLastError() == ERROR_ALREADY_EXISTS;
        }

        bool Stamp(const std::wstring& path, FILETIME& timestamp)
        {
            WIN32_FILE_ATTRIBUTE_DATA item{};
            if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &item) ||
                (item.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
                item.nFileSizeHigh || item.nFileSizeLow != BYTES) return false;
            if (CompareFileTime(&item.ftLastWriteTime, &timestamp) > 0)
                timestamp = item.ftLastWriteTime;
            return true;
        }

        bool Valid(const std::wstring& path, const FILETIME& sources)
        {
            FILETIME timestamp{};
            return Stamp(path, timestamp) &&
                CompareFileTime(&timestamp, &sources) >= 0;
        }

        void Add(BYTE* destination, const BYTE* layer)
        {
            for (UINT x = 0; x < STRIDE; x += 4)
            {
                const UINT a = layer[x + 3];
                if (!a) continue;
                const UINT remain = 255 - a;
                for (UINT c = 0; c < 4; ++c)
                    destination[x + c] = BYTE(layer[x + c] +
                        (UINT(destination[x + c]) * remain + 127) / 255);
            }
        }

        bool Compose(const GroupNode& group, const std::wstring& path)
        {
            if (!group.scene || !group.grass || !group.straw ||
                !group.scene->ready || !group.grass->ready || !group.straw->ready)
                return false;
            FILETIME newest{};
            if (!Stamp(group.scene->path, newest) ||
                !Stamp(group.grass->path, newest) ||
                !Stamp(group.straw->path, newest)) return false;
            if (Valid(path, newest)) return true;

            FILE* scene = nullptr;
            FILE* grass = nullptr;
            FILE* straw = nullptr;
            FILE* output = nullptr;
            const std::wstring temporary = path + L".tmp";
            bool ok = _wfopen_s(&scene, group.scene->path.c_str(), L"rb") == 0 && scene;
            if (ok) ok = _wfopen_s(&grass, group.grass->path.c_str(), L"rb") == 0 && grass;
            if (ok) ok = _wfopen_s(&straw, group.straw->path.c_str(), L"rb") == 0 && straw;
            if (ok) ok = _wfopen_s(&output, temporary.c_str(), L"wb") == 0 && output;
            BYTE row[STRIDE], layer[STRIDE];
            for (UINT y = 0; y < HEIGHT && ok; ++y)
            {
                ok = fread(row, 1, STRIDE, scene) == STRIDE &&
                    fread(layer, 1, STRIDE, grass) == STRIDE;
                if (!ok) break;
                Add(row, layer);
                ok = fread(layer, 1, STRIDE, straw) == STRIDE;
                if (!ok) break;
                Add(row, layer);
                ok = fwrite(row, 1, STRIDE, output) == STRIDE;
            }
            if (scene && fclose(scene) != 0) ok = false;
            if (grass && fclose(grass) != 0) ok = false;
            if (straw && fclose(straw) != 0) ok = false;
            if (output && fclose(output) != 0) ok = false;
            if (ok && MoveFileExW(temporary.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temporary.c_str());
            return false;
        }

        // A 100%-to-0% bushel stage is composed ONCE from existing Math caches.
        // Only the finished stage cache travels to Prime; not ten layer passes
        // on every 33 ms animation tick. Premultiplied BGRA stays unchanged.
        bool ComposeBushelStage(HitNode& stage)
        {
            FILETIME latest{};
            for (UINT i = 0; i < 5; ++i)
            {
                if (stage.grassSigns[i] == Polarity::Plus &&
                    (!stage.grassLayers[i] || !stage.grassLayers[i]->ready ||
                        !Stamp(stage.grassLayers[i]->path, latest))) return false;
                if (stage.strawSigns[i] == Polarity::Plus &&
                    (!stage.strawLayers[i] || !stage.strawLayers[i]->ready ||
                        !Stamp(stage.strawLayers[i]->path, latest))) return false;
            }
            if (Valid(stage.overlay.path, latest))
                return stage.overlay.ready = true;

            FILE* sources[10]{};
            FILE* output = nullptr;
            bool ok = true;
            for (UINT i = 0; i < 5 && ok; ++i)
            {
                if (stage.grassSigns[i] == Polarity::Plus)
                    ok = _wfopen_s(&sources[2 * i],
                        stage.grassLayers[i]->path.c_str(), L"rb") == 0 && sources[2 * i];
                if (ok && stage.strawSigns[i] == Polarity::Plus)
                    ok = _wfopen_s(&sources[2 * i + 1],
                        stage.strawLayers[i]->path.c_str(), L"rb") == 0 && sources[2 * i + 1];
            }
            const std::wstring temp = stage.overlay.path + L".tmp";
            if (ok) ok = _wfopen_s(&output, temp.c_str(), L"wb") == 0 && output;

            BYTE composed[STRIDE]{}, layer[STRIDE]{};
            for (UINT y = 0; y < HEIGHT && ok; ++y)
            {
                ZeroMemory(composed, STRIDE);
                for (FILE* source : sources)
                {
                    if (!source) continue;
                    if (fread(layer, 1, STRIDE, source) != STRIDE)
                    {
                        ok = false;
                        break;
                    }
                    Add(composed, layer);
                }
                if (ok) ok = fwrite(composed, 1, STRIDE, output) == STRIDE;
            }
            for (FILE* source : sources)
                if (source && fclose(source) != 0) ok = false;
            if (output && fclose(output) != 0) ok = false;
            if (ok && MoveFileExW(temp.c_str(), stage.overlay.path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return stage.overlay.ready = true;
            DeleteFileW(temp.c_str());
            return false;
        }

        bool Initialize(const SAO_Harvest_Tall_Grass_Math::OutputNode& input)
        {
            std::wstring folder;
            if (!Directory(folder)) return false;
            Cache.grassItem = &input.grassItem;
            Cache.strawItem = &input.strawItem;

            // Only numeric reward counts flow forward. No reward item sprites
            // are added to an animated scene; the item PNGs are cached sources.
            for (UINT i = 0; i <= 10; ++i)
            {
                HitNode& stage = Cache.hit[i];
                stage.overlay.name = L"SAO_Harvest_Bushel_Group_v2_" + std::to_wstring(i);
                stage.overlay.path = folder + stage.overlay.name + L".cache";
                stage.damagePercent = i * 10;
                stage.remainingPercent = 100 - stage.damagePercent;
                stage.strawReward = (i + 1) / 2; // 1st, 3rd, 5th, 7th, 9th hits
                stage.grassReward = i / 2;       // 2nd, 4th, 6th, 8th, 10th hits
                stage.straw = stage.strawReward < 5 ? &input.straw[stage.strawReward] : nullptr;
                stage.grass = stage.grassReward < 5 ? &input.grass[stage.grassReward] : nullptr;

                // All ten PNGs are one additive group at 100% health.
                // Each 10% threshold subtracts exactly one alternating layer:
                // -Straw0, -Grass0, -Straw1, -Grass1, ... -Grass4.
                for (UINT piece = 0; piece < 5; ++piece)
                {
                    stage.strawLayers[piece] = &input.straw[piece];
                    stage.grassLayers[piece] = &input.grass[piece];
                    stage.strawSigns[piece] = piece >= stage.strawReward ?
                        Polarity::Plus : Polarity::Minus;
                    stage.grassSigns[piece] = piece >= stage.grassReward ?
                        Polarity::Plus : Polarity::Minus;
                }
            }
            for (UINT i = 0; i < SAO_Harvest_Tall_Grass_Math::COUNT; ++i)
            {
                StepNode& step = Cache.steps[i];
                step.group.scene = &input.scenes[i];
                step.group.grass = Cache.hit[0].grass;
                step.group.straw = Cache.hit[0].straw;
                step.frame.name = L"SAO_Harvest_Tall_Grass_Projection_" + std::to_wstring(i);
                step.frame.path = folder + step.frame.name + L".cache";
                step.next = (i + 1) % SAO_Harvest_Tall_Grass_Math::COUNT;
            }
            return true;
        }

        bool Prepare(UINT first, UINT last)
        {
            for (UINT i = first; i < last; ++i)
            {
                StepNode& step = Cache.steps[i];
                step.frame.ready = Compose(step.group, step.frame.path);
                if (!step.frame.ready) return false;
            }
            return true;
        }
    }

    const OutputNode& Build(const SAO_Harvest_Tall_Grass_Math::OutputNode& input, HWND target)
    {
        if (!initialized)
        {
            initialized = true;
            Cache.ready = input.ready && Initialize(input) &&
                ComposeBushelStage(Cache.hit[0]) && Prepare(0, 1);
            if (!Cache.ready) Cache.error = input.error.empty() ?
                L"Harvest's first conductor cache failed." : input.error;
            SAO_Harvest_Tall_Grass_Conductor_Cache_Node::Build(Cache, target);
        }
        else if (!Cache.complete.load(std::memory_order_acquire))
        {
            bool stagesReady = input.ready;
            for (UINT i = 1; i <= 10 && stagesReady; ++i)
                stagesReady = ComposeBushelStage(Cache.hit[i]);
            Cache.animation_ready = stagesReady &&
                Prepare(1, SAO_Harvest_Tall_Grass_Math::COUNT);
            if (!Cache.animation_ready) Cache.preload_error = input.error.empty() ?
                L"Harvest's animation conductor caches failed." : input.error;
            Cache.complete.store(true, std::memory_order_release);
            SAO_Harvest_Tall_Grass_Conductor_Cache_Node::Build(Cache, target);
        }
        return Cache;
    }
}