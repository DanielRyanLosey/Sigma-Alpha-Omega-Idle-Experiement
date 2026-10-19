#include <windows.h>
#include <initguid.h>
#include <wincodec.h>
#include <filesystem>
#include <cstdio>
#include <vector>
#include "SAO_Main_Title_Math.h"

#pragma comment(lib, "ole32.lib")

SAO_Main_Title_Math::OutputNode SAO_Main_Title_Math::Cache{};

namespace
{
    constexpr UINT WIDTH = 1920, HEIGHT = 1080;
    constexpr UINT BYTES = WIDTH * HEIGHT * 4;
    constexpr const wchar_t* NAMES[] = {
        L"Title_Symbol_Main_Alpha.png",
        L"Title_Symbol_Main_Omega.png",
        L"Title_Symbol_Main_Sigma.png"
    };
}

static bool Cached(const std::wstring& png, const std::wstring& path)
{
    WIN32_FILE_ATTRIBUTE_DATA source{}, cache{};
    return GetFileAttributesExW(png.c_str(), GetFileExInfoStandard, &source) &&
        GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &cache) &&
        !(cache.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
        cache.nFileSizeHigh == 0 && cache.nFileSizeLow == BYTES &&
        CompareFileTime(&cache.ftLastWriteTime, &source.ftLastWriteTime) >= 0;
}

static bool Decode(IWICImagingFactory* factory,
    const std::wstring& file, BYTE* pixels)
{
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* bitmap = nullptr;
    HRESULT result = factory->CreateDecoderFromFilename(file.c_str(), nullptr,
        GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    UINT width = 0, height = 0;
    if (SUCCEEDED(result)) result = frame->GetSize(&width, &height);
    if (SUCCEEDED(result) && (width != WIDTH || height != HEIGHT)) result = E_FAIL;
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&bitmap);
    if (SUCCEEDED(result))
        result = bitmap->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
    if (SUCCEEDED(result))
        result = bitmap->CopyPixels(nullptr, WIDTH * 4, BYTES, pixels);
    if (bitmap) bitmap->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    return SUCCEEDED(result);
}

const SAO_Main_Title_Math::OutputNode& SAO_Main_Title_Math::Build()
{
    if (SAO_Main_Title_Math::Cache.ready) return SAO_Main_Title_Math::Cache;
    SAO_Main_Title_Math::Cache.error.clear();

    wchar_t executable[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (!length || length >= MAX_PATH)
    {
        SAO_Main_Title_Math::Cache.error = L"Cannot locate the executable.";
        return SAO_Main_Title_Math::Cache;
    }
    std::wstring folder(executable, length);
    folder.resize(folder.find_last_of(L"\\/") + 1);
    std::wstring cacheFolder = folder + L"Cache\\";
    if (!CreateDirectoryW(cacheFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SAO_Main_Title_Math::Cache.error = L"Cannot create: " + cacheFolder;
        return SAO_Main_Title_Math::Cache;
    }

    cacheFolder += L"SAO_Main_Title_Math\\";
    if (!CreateDirectoryW(cacheFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SAO_Main_Title_Math::Cache.error = L"Cannot create: " + cacheFolder;
        return SAO_Main_Title_Math::Cache;
    }
    namespace fs = std::filesystem;
    auto root = fs::path(folder);
    fs::path assets;
    for (int i = 0; i < 8 && !root.empty(); ++i)
    {
        std::error_code ec;
        auto candidate = root / L"SAO_I_E Assets";
        if (fs::is_directory(candidate, ec)) { assets = candidate; break; }
        auto parent = root.parent_path();
        if (parent == root) break;
        root = parent;
    }
    if (assets.empty())
    {
        SAO_Main_Title_Math::Cache.error = L"Cannot locate SAO_I_E Assets beside the executable or in its parent folders.";
        return SAO_Main_Title_Math::Cache;
    }
    const std::wstring sourceFolder = (assets / LR"(Main Screen\Title Symbol Main\)").wstring();

    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
    {
        SAO_Main_Title_Math::Cache.error = L"Cannot initialize Windows PNG decoding.";
        return SAO_Main_Title_Math::Cache;
    }
    IWICImagingFactory* factory = nullptr;
    bool success = true;
    std::vector<BYTE> pixels;

    for (unsigned i = 0; i < COUNT && success; ++i)
    {
        auto& cache = SAO_Main_Title_Math::Cache.layers[i];
        cache.name = NAMES[i];
        cache.path = cacheFolder + cache.name + L".cache";
        std::wstring png = sourceFolder + cache.name;
        cache.ready = Cached(png, cache.path);
        if (cache.ready) continue;
        if (!factory && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
            CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        {
            success = false;
            break;
        }
        if (pixels.empty()) pixels.resize(BYTES);
        success = Decode(factory, png, pixels.data());
        if (!success) { SAO_Main_Title_Math::Cache.error = L"PNG decode failed (1920x1080 required): " + png; break; }
        FILE* output = nullptr;
        success = _wfopen_s(&output, cache.path.c_str(), L"wb") == 0 && output;
        if (success)
        {
            const bool written = fwrite(pixels.data(), 1, BYTES, output) == BYTES;
            success = fclose(output) == 0 && written;
            if (!success) DeleteFileW(cache.path.c_str());
        }
        if (!success) SAO_Main_Title_Math::Cache.error = L"Cache write failed: " + cache.path;
        cache.ready = success;
    }
    if (factory) factory->Release();
    if (SUCCEEDED(com)) CoUninitialize();
    SAO_Main_Title_Math::Cache.ready = success;
    if (!success && SAO_Main_Title_Math::Cache.error.empty())
        SAO_Main_Title_Math::Cache.error = L"Cannot create the Windows PNG decoder.";
    return SAO_Main_Title_Math::Cache;
}



