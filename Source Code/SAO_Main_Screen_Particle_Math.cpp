#include <windows.h>
#include <initguid.h>
#include <wincodec.h>
#include <filesystem>
#include <cstdio>
#include <vector>
#include "SAO_Main_Screen_Particle_Math.h"

#pragma comment(lib, "ole32.lib")

SAO_Main_Screen_Particle_Math::OutputNode SAO_Main_Screen_Particle_Math::Cache{};

namespace
{
    constexpr UINT WIDTH = 1920, HEIGHT = 1080;
    constexpr UINT BYTES = WIDTH * HEIGHT * 4;
    constexpr const wchar_t* NAMES[] = {
        L"Particle_Main_1.png",
        L"Particle_Main_2.png",
        L"Particle_Main_3.png",
        L"Particle_Main_4.png",
        L"Particle_Main_5.png",
        L"Particle_Main_6.png",
        L"Particle_Main_7.png",
        L"Particle_Main_8.png",
        L"Particle_Main_9.png",
        L"Particle_Main_10.png",
        L"Particle_Main_11.png",
        L"Particle_Main_12.png",
        L"Particle_Main_13.png",
        L"Particle_Main_14.png",
        L"Particle_Main_15.png",
        L"Particle_Main_16.png",
        L"Particle_Main_17.png",
        L"Particle_Main_18.png",
        L"Particle_Main_19.png",
        L"Particle_Main_20.png",
        L"Particle_Main_21.png",
        L"Particle_Main_22.png",
        L"Particle_Main_23.png",
        L"Particle_Main_24.png",
        L"Particle_Main_25.png",
        L"Particle_Main_26.png",
        L"Particle_Main_27.png",
        L"Particle_Main_28.png",
        L"Particle_Main_29.png",
        L"Particle_Main_30.png",
        L"Particle_Main_31.png",
        L"Particle_Main_32.png",
        L"Particle_Main_33.png",
        L"Particle_Main_34.png",
        L"Particle_Main_35.png",
        L"Particle_Main_36.png",
        L"Particle_Main_37.png",
        L"Particle_Main_38.png",
        L"Particle_Main_39.png",
        L"Particle_Main_40.png",
        L"Particle_Main_41.png",
        L"Particle_Main_42.png",
        L"Particle_Main_43.png",
        L"Particle_Main_44.png",
        L"Particle_Main_45.png"
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

const SAO_Main_Screen_Particle_Math::OutputNode& SAO_Main_Screen_Particle_Math::Build(bool preload)
{

    SAO_Main_Screen_Particle_Math::Cache.ready = false;
    SAO_Main_Screen_Particle_Math::Cache.error.clear();

    wchar_t executable[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (!length || length >= MAX_PATH)
    {
        SAO_Main_Screen_Particle_Math::Cache.error = L"Cannot locate the executable.";
        return SAO_Main_Screen_Particle_Math::Cache;
    }
    std::wstring folder(executable, length);
    folder.resize(folder.find_last_of(L"\\/") + 1);
    std::wstring cacheFolder = folder + L"Cache\\";
    if (!CreateDirectoryW(cacheFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SAO_Main_Screen_Particle_Math::Cache.error = L"Cannot create: " + cacheFolder;
        return SAO_Main_Screen_Particle_Math::Cache;
    }

    cacheFolder += L"SAO_Main_Screen_Particle_Math\\";
    if (!CreateDirectoryW(cacheFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SAO_Main_Screen_Particle_Math::Cache.error = L"Cannot create: " + cacheFolder;
        return SAO_Main_Screen_Particle_Math::Cache;
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
        SAO_Main_Screen_Particle_Math::Cache.error = L"Cannot locate SAO_I_E Assets beside the executable or in its parent folders.";
        return SAO_Main_Screen_Particle_Math::Cache;
    }
    const std::wstring sourceFolder = (assets / LR"(Main Screen\Particle Main\)").wstring();

    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
    {
        SAO_Main_Screen_Particle_Math::Cache.error = L"Cannot initialize Windows PNG decoding.";
        return SAO_Main_Screen_Particle_Math::Cache;
    }
    IWICImagingFactory* factory = nullptr;
    bool success = true;
    std::vector<BYTE> pixels;

    for (unsigned i = 0; i < COUNT && success; ++i)
    {
        if (!preload && i != 0) continue;
        if (SAO_Main_Screen_Math::Cancelled()) { success = false; break; }
        auto& cache = SAO_Main_Screen_Particle_Math::Cache.layers[i];
        if (cache.ready) continue;
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
        if (!success) { SAO_Main_Screen_Particle_Math::Cache.error = L"PNG decode failed (1920x1080 required): " + png; break; }
        FILE* output = nullptr;
        success = _wfopen_s(&output, cache.path.c_str(), L"wb") == 0 && output;
        if (success)
        {
            const bool written = fwrite(pixels.data(), 1, BYTES, output) == BYTES;
            success = fclose(output) == 0 && written;
            if (!success) DeleteFileW(cache.path.c_str());
        }
        if (!success) SAO_Main_Screen_Particle_Math::Cache.error = L"Cache write failed: " + cache.path;
        cache.ready = success;
    }
    if (factory) factory->Release();
    if (SUCCEEDED(com)) CoUninitialize();
    SAO_Main_Screen_Particle_Math::Cache.ready = success;
    if (!success && SAO_Main_Screen_Particle_Math::Cache.error.empty())
        SAO_Main_Screen_Particle_Math::Cache.error = L"Cannot create the Windows PNG decoder.";
    return SAO_Main_Screen_Particle_Math::Cache;
}





