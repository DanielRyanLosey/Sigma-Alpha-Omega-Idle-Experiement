#include "SAO_Base_Harvest_Action_Torso_Math.h"
#include "SAO_Base_Harvest_Action_Torso_Conductor.h"
#include <filesystem>
#include <wincodec.h>
#include <cstdio>

#pragma comment(lib, "ole32.lib")

namespace SAO_Base_Harvest_Action_Torso_Math
{
    OutputNode Cache{};
    namespace
    {
        using namespace SAO_Character_Harvest;

        std::wstring SourceFolder()
        {
            namespace fs = std::filesystem;
            wchar_t executable[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
            if (!n || n >= MAX_PATH) return {};
            auto root = fs::path(std::wstring(executable, n)).parent_path();
            for (int i = 0; i < 8 && !root.empty(); ++i)
            {
                std::error_code ec;
                const auto assets = root / L"SAO_I_E Assets";
                if (fs::is_directory(assets, ec))
                    return (assets / LR"(Character\Base Harvest Action\Base Harvest Action Torso\)").wstring();
                auto parent = root.parent_path();
                if (parent == root) break;
                root = parent;
            }
            return {};
        }

        std::wstring CacheFolder()
        {
            wchar_t executable[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
            if (!n || n >= MAX_PATH) return {};
            std::wstring folder(executable, n);
            folder.resize(folder.find_last_of(L"\\/") + 1);
            folder += L"Cache\\";
            if (!CreateDirectoryW(folder.c_str(), nullptr) &&
                GetLastError() != ERROR_ALREADY_EXISTS) return {};
            folder += L"SAO_Base_Harvest_Action_Torso_Math\\";
            if (!CreateDirectoryW(folder.c_str(), nullptr) &&
                GetLastError() != ERROR_ALREADY_EXISTS) return {};
            return folder;
        }

        bool Fresh(const std::wstring& png, const std::wstring& saved)
        {
            WIN32_FILE_ATTRIBUTE_DATA source{}, cache{};
            return GetFileAttributesExW(png.c_str(), GetFileExInfoStandard, &source) &&
                GetFileAttributesExW(saved.c_str(), GetFileExInfoStandard, &cache) &&
                !(cache.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                cache.nFileSizeHigh == 0 && cache.nFileSizeLow == BYTES &&
                CompareFileTime(&cache.ftLastWriteTime, &source.ftLastWriteTime) >= 0;
        }

        bool Bitmap(IWICImagingFactory* factory,
            const std::wstring& png, const std::wstring& saved)
        {
            IWICBitmapDecoder* decoder = nullptr;
            IWICBitmapFrameDecode* frame = nullptr;
            IWICFormatConverter* converter = nullptr;
            HRESULT hr = factory->CreateDecoderFromFilename(png.c_str(), nullptr,
                GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
            if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
            UINT width = 0, height = 0;
            if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);
            if (SUCCEEDED(hr) && (width != WIDTH || height != HEIGHT)) hr = E_FAIL;
            if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
            if (SUCCEEDED(hr)) hr = converter->Initialize(frame,
                GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                nullptr, 0, WICBitmapPaletteTypeCustom);

            const std::wstring temp = saved + L".tmp";
            FILE* output = nullptr;
            if (SUCCEEDED(hr) &&
                (_wfopen_s(&output, temp.c_str(), L"wb") != 0 || !output)) hr = E_FAIL;
            BYTE row[STRIDE]{};
            for (UINT y = 0; SUCCEEDED(hr) && y < HEIGHT; ++y)
            {
                WICRect rect{ 0, static_cast<INT>(y), static_cast<INT>(WIDTH), 1 };
                hr = converter->CopyPixels(&rect, STRIDE, STRIDE, row);
                if (SUCCEEDED(hr) && fwrite(row, 1, STRIDE, output) != STRIDE)
                    hr = E_FAIL;
            }
            if (output && fclose(output) != 0) hr = E_FAIL;
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (SUCCEEDED(hr) && MoveFileExW(temp.c_str(), saved.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temp.c_str());
            return false;
        }
    }

    const OutputNode& Build(bool preload)
    {
        using namespace SAO_Character_Harvest;
        const std::wstring source = SourceFolder();
        const std::wstring target = CacheFolder();
        if (source.empty() || target.empty())
        {
            Cache.error = L"SAO_Base_Harvest_Action_Torso_Math: folder unavailable.";
            return Cache;
        }
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com) && com != RPC_E_CHANGED_MODE)
        {
            Cache.error = L"SAO_Base_Harvest_Action_Torso_Math: COM initialization failed.";
            return Cache;
        }
        IWICImagingFactory* factory = nullptr;
        bool ok = true;
        const UINT total = preload ? COUNT : 1;
        for (UINT i = 0; i < total && ok; ++i)
        {
            auto& node = Cache.frames[i];
            wchar_t filename[96]{};
            swprintf_s(filename, L"SAO_Harvest_Action_Torso_%04u.png", i + 1);
            node.name = filename;
            node.path = target + node.name + L".cache";
            const std::wstring png = source + node.name;
            if (!Fresh(png, node.path))
            {
                if (!factory) ok = SUCCEEDED(CoCreateInstance(
                    CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                    IID_PPV_ARGS(&factory)));
                if (ok) ok = Bitmap(factory, png, node.path);
            }
            node.ready = ok;
            if (!ok) Cache.error = L"SAO_Base_Harvest_Action_Torso_Math: missing/invalid PNG: " + png;
        }
        if (factory) factory->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        Cache.ready = Cache.frames[0].ready;
        if (preload) Cache.complete = ok;
        if (ok) SAO_Base_Harvest_Action_Torso_Conductor::Build(Cache);
        return Cache;
    }
}