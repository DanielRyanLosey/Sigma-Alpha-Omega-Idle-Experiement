#include "SAO_Character_Prime_Conductor.h"
#include "SAO_Character_Prime_Conductor_Cache_Node.h"
#include <cstdio>
#include <array>
#include <algorithm>

namespace SAO_Character_Prime_Conductor
{
    OutputNode Cache{};
    namespace
    {
        using namespace SAO_Character_Harvest;
        // +Head +LeftArm +Torso +RightArm +Legs +Feet +Tool.
        // Original 1920x1080 positions and matching 23-frame indices.
        constexpr std::array<UINT, PARTS> ORDER{
            static_cast<UINT>(Part::Head),
            static_cast<UINT>(Part::LUA),
            static_cast<UINT>(Part::LFA),
            static_cast<UINT>(Part::LH),
            static_cast<UINT>(Part::Torso),
            static_cast<UINT>(Part::RUA),
            static_cast<UINT>(Part::RFA),
            static_cast<UINT>(Part::RH),
            static_cast<UINT>(Part::Legs),
            static_cast<UINT>(Part::Feet),
            static_cast<UINT>(Part::Tool)
        };

        std::wstring Folder()
        {
            wchar_t executable[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
            if (!n || n >= MAX_PATH) return {};
            std::wstring folder(executable, n);
            folder.resize(folder.find_last_of(L"\\/") + 1);
            folder += L"Cache\\";
            if (!CreateDirectoryW(folder.c_str(), nullptr) &&
                GetLastError() != ERROR_ALREADY_EXISTS) return {};
            folder += L"SAO_Character_Prime_Conductor\\";
            if (!CreateDirectoryW(folder.c_str(), nullptr) &&
                GetLastError() != ERROR_ALREADY_EXISTS) return {};
            return folder;
        }

        bool Timestamp(const BitmapNode& bitmap, FILETIME& latest)
        {
            WIN32_FILE_ATTRIBUTE_DATA attributes{};
            if (!bitmap.ready || !GetFileAttributesExW(bitmap.path.c_str(),
                GetFileExInfoStandard, &attributes) || attributes.nFileSizeHigh ||
                attributes.nFileSizeLow != BYTES) return false;
            if (CompareFileTime(&latest, &attributes.ftLastWriteTime) < 0)
                latest = attributes.ftLastWriteTime;
            return true;
        }

        bool Compose(StepNode& step)
        {
            FILETIME latest{};
            for (const auto* layer : step.layers)
                if (!layer || !Timestamp(*layer, latest)) return false;
            WIN32_FILE_ATTRIBUTE_DATA file{};
            if (GetFileAttributesExW(step.frame.path.c_str(), GetFileExInfoStandard, &file) &&
                !file.nFileSizeHigh && file.nFileSizeLow == BYTES &&
                CompareFileTime(&file.ftLastWriteTime, &latest) >= 0)
                return step.frame.ready = true;

            std::array<FILE*, PARTS> sources{};
            FILE* output = nullptr;
            bool ok = true;
            for (UINT i = 0; i < PARTS && ok; ++i)
                ok = _wfopen_s(&sources[i], step.layers[i]->path.c_str(), L"rb") == 0 && sources[i];
            const std::wstring temporary = step.frame.path + L".tmp";
            if (ok) ok = _wfopen_s(&output, temporary.c_str(), L"wb") == 0 && output;
            BYTE row[STRIDE]{}, layer[STRIDE]{};
            for (UINT y = 0; ok && y < HEIGHT; ++y)
            {
                ZeroMemory(row, STRIDE);
                for (UINT i = 0; i < PARTS && ok; ++i)
                {
                    ok = fread(layer, 1, STRIDE, sources[i]) == STRIDE;
                    if (!ok) break;
                    if (step.signs[i] == Polarity::Minus) continue;
                    for (UINT x = 0; x < STRIDE; x += 4)
                    {
                        const UINT a = layer[x + 3];
                        if (!a) continue;
                        const UINT inverse = 255 - a;
                        for (UINT channel = 0; channel < 4; ++channel)
                        {
                            const UINT value = UINT(layer[x + channel]) +
                                (UINT(row[x + channel]) * inverse + 127) / 255;
                            row[x + channel] = BYTE((std::min)(255u, value));
                        }
                    }
                }
                if (ok) ok = fwrite(row, 1, STRIDE, output) == STRIDE;
            }
            for (auto* source : sources)
                if (source && fclose(source) != 0) ok = false;
            if (output && fclose(output) != 0) ok = false;
            if (ok && MoveFileExW(temporary.c_str(), step.frame.path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return step.frame.ready = true;
            DeleteFileW(temporary.c_str());
            return false;
        }
    }

    const OutputNode& Build(const SAO_Character_Harvest_Stats::OutputNode& input, HWND target)
    {
        if (!input.ready || !target) return Cache;
        Cache.stats = &input;
        const std::wstring directory = Folder();
        if (directory.empty())
        {
            Cache.error = L"Character Prime cache folder unavailable.";
            return Cache;
        }
        const UINT limit = input.complete ? COUNT : 1;
        bool ok = true;
        for (UINT frame = 0; frame < limit && ok; ++frame)
        {
            auto& step = Cache.steps[frame];
            step.frame.name = L"SAO_Character_Harvest_Body_No_Tool_v6_" + std::to_wstring(frame + 1);
            step.frame.path = directory + step.frame.name + L".cache";
            step.next = (frame + 1) % COUNT;
            for (UINT i = 0; i < PARTS; ++i)
            {
                step.layers[i] = input.parts[ORDER[i]]->steps[frame].frame;
                step.signs[i] = input.parts[ORDER[i]]->steps[frame].sign;
            }
            // Forward oak separately: body cache contains no tool pixels.
            for (UINT i = 0; i < PARTS; ++i)if (ORDER[i] == static_cast<UINT>(Part::Tool)) {
                step.oakTool = step.layers[i]; step.oakSign = step.signs[i];
                step.signs[i] = Polarity::Minus;
            }
            ok = Compose(step);
        }
        Cache.ready = Cache.steps[0].frame.ready;
        Cache.complete = input.complete && ok;
        if (!ok) Cache.error = L"Character Prime composition failed.";
        if (ok) SAO_Character_Prime_Conductor_Cache_Node::Build(Cache, target);
        return Cache;
    }
}

