#include "SAO_Symbol_Medium_Math.h"
#include <windows.h>
#include <initguid.h>
#include <wincodec.h>
#include <shlobj.h>
#include <cstdio>
#include <climits>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

SAO_Symbol_Medium_Math::OutputNode SAO_Symbol_Medium_Math::Cache{};

namespace
{
    // Exact 16 filenames, in the order of the supplied Symbol Medium directory.
    constexpr const wchar_t* PNG_NAMES[SAO_Symbol_Medium_Math::COUNT] =
    {
        L"SAO_Symbol_Alpha_Lower_Medium.png",
        L"SAO_Symbol_Degree_Medium.png",
        L"SAO_Symbol_Delta_Medium.png",
        L"SAO_Symbol_Divide_Medium.png",
        L"SAO_Symbol_Greater_Equal_Medium.png",
        L"SAO_Symbol_Infinity_Medium.png",
        L"SAO_Symbol_Less_Equal_Medium.png",
        L"SAO_Symbol_Multiply_Medium.png",
        L"SAO_Symbol_Not_Equal_Medium.png",
        L"SAO_Symbol_Omega_Lower_Medium.png",
        L"SAO_Symbol_Omega_Medium.png",
        L"SAO_Symbol_Pi_Medium.png",
        L"SAO_Symbol_Plus_Minus_Medium.png",
        L"SAO_Symbol_Sigma_Medium.png",
        L"SAO_Symbol_Square_Root_Medium.png",
        L"SAO_Symbol_Theta_Medium.png"
    };

    bool Folder(const std::wstring& path)
    {
        if (CreateDirectoryW(path.c_str(), nullptr)) return true;
        return GetLastError() == ERROR_ALREADY_EXISTS;
    }

    bool Locations(std::wstring& pngFolder, std::wstring& cacheFolder)
    {
        wchar_t executable[MAX_PATH]{};
        const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (!n || n >= MAX_PATH) return false;

        std::wstring executableFolder(executable, n);
        executableFolder.resize(executableFolder.find_last_of(L"\\/") + 1);
        cacheFolder = executableFolder + L"Cache\\";
        if (!Folder(cacheFolder)) return false;
        cacheFolder += L"SAO_Symbol_Medium_Math\\";
        if (!Folder(cacheFolder)) return false;

        // Locate assets relative to the executable; never use OneDrive's Desktop.
        std::wstring assetRoot;
        std::wstring parent = executableFolder;
        for (unsigned depth = 0; depth < 8; ++depth)
        {
            const std::wstring candidate = parent + L"SAO_I_E Assets\\";
            const DWORD flags = GetFileAttributesW(candidate.c_str());
            if (flags != INVALID_FILE_ATTRIBUTES && (flags & FILE_ATTRIBUTE_DIRECTORY))
            {
                assetRoot = candidate;
                break;
            }
            if (parent.empty()) break;
            parent.pop_back();
            const auto slash = parent.find_last_of(L"\\/");
            if (slash == std::wstring::npos) break;
            parent.resize(slash + 1);
        }
        if (assetRoot.empty()) return false;
        pngFolder = assetRoot + L"Alphabet\\Symbols\\Symbol Medium\\";
        return true;
    }

    bool Fresh(const std::wstring& source, const std::wstring& cached, UINT bytes)
    {
        WIN32_FILE_ATTRIBUTE_DATA png{}, bitmap{};
        return GetFileAttributesExW(source.c_str(), GetFileExInfoStandard, &png) &&
            !(png.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            GetFileAttributesExW(cached.c_str(), GetFileExInfoStandard, &bitmap) &&
            !(bitmap.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            bitmap.nFileSizeHigh == 0 && bitmap.nFileSizeLow == bytes &&
            CompareFileTime(&bitmap.ftLastWriteTime, &png.ftLastWriteTime) >= 0;
    }

    bool Save(const std::wstring& target, const BYTE* data, UINT bytes)
    {
        const std::wstring temporary = target + L".tmp";
        FILE* file = nullptr;
        if (_wfopen_s(&file, temporary.c_str(), L"wb") != 0 || !file)
            return false;
        const bool written = fwrite(data, 1, bytes, file) == bytes;
        const bool closed = fclose(file) == 0;
        if (written && closed && MoveFileExW(temporary.c_str(), target.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;
        DeleteFileW(temporary.c_str());
        return false;
    }

    bool ConvertOne(IWICImagingFactory* factory,
        const std::wstring& png, SAO_Symbol_Medium_Math::BitmapCacheNode& node,
        std::vector<BYTE>& pixels, std::wstring& error)
    {
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;
        HRESULT hr = factory->CreateDecoderFromFilename(png.c_str(), nullptr,
            GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        UINT width = 0, height = 0;
        if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);
        if (SUCCEEDED(hr) && (!width || !height || width > UINT_MAX / 4 ||
            height > UINT_MAX / (width * 4))) hr = E_FAIL;

        if (SUCCEEDED(hr))
        {
            node.width = width;
            node.height = height;
            node.bytes = width * height * 4;
            node.ready = Fresh(png, node.path, node.bytes);
            if (!node.ready)
            {
                hr = factory->CreateFormatConverter(&converter);
                if (SUCCEEDED(hr)) hr = converter->Initialize(frame,
                    GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                    nullptr, 0, WICBitmapPaletteTypeCustom);
                if (SUCCEEDED(hr))
                {
                    pixels.resize(node.bytes);
                    hr = converter->CopyPixels(nullptr, width * 4, node.bytes, pixels.data());
                }
                if (SUCCEEDED(hr) && !Save(node.path, pixels.data(), node.bytes))
                    hr = E_FAIL;
                node.ready = SUCCEEDED(hr);
            }
        }
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (FAILED(hr)) error = L"Cannot decode or cache PNG: " + png;
        return SUCCEEDED(hr);
    }
}

const SAO_Symbol_Medium_Math::OutputNode& SAO_Symbol_Medium_Math::Build()
{
    if (Cache.ready) return Cache;
    Cache = OutputNode{};

    std::wstring pngFolder, cacheFolder;
    if (!Locations(pngFolder, cacheFolder))
    {
        Cache.error = L"Cannot locate Symbol Medium PNGs or create the cache folder.";
        return Cache;
    }

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
    {
        Cache.error = L"Windows COM initialization failed.";
        return Cache;
    }
    IWICImagingFactory* factory = nullptr;
    const HRESULT created = CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(created))
    {
        Cache.error = L"Windows WIC bitmap decoder creation failed.";
        if (SUCCEEDED(com)) CoUninitialize();
        return Cache;
    }

    std::vector<BYTE> scratch; // Reuses one bitmap buffer, not 16 resident bitmaps.
    bool all = true;
    for (std::size_t i = 0; i < COUNT; ++i)
    {
        BitmapCacheNode& node = Cache.symbols[i];
        node.name = PNG_NAMES[i];
        node.path = cacheFolder + node.name + L".cache";
        if (!ConvertOne(factory, pngFolder + node.name, node, scratch, Cache.error))
        {
            all = false;
            break;
        }
    }
    factory->Release();
    if (SUCCEEDED(com)) CoUninitialize();
    Cache.ready = all;
    return Cache;
}