#include "SAO_Toth_Minimized_Overlay_Math.h"
#include <windows.h>
#include <initguid.h>
#include <wincodec.h>
#include <shlobj.h>
#include <cstdio>
#include <climits>
#include <vector>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

SAO_Toth_Minimized_Overlay_Math::OutputNode SAO_Toth_Minimized_Overlay_Math::Cache{};

namespace
{
    constexpr const wchar_t* PNG_NAMES[SAO_Toth_Minimized_Overlay_Math::COUNT] =
    {
        L"SAO_Toth_Minimized_Input_Enter_Button.png",
        L"SAO_Toth_Minimized_Input_Frame.png",
        L"SAO_Toth_Minimized_Input_Location.png"
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
        if (!n || n >= MAX_PATH)
            return false;

        std::wstring executableFolder(executable, n);
        const std::size_t separator = executableFolder.find_last_of(L"\\/");
        if (separator == std::wstring::npos)
            return false;
        executableFolder.resize(separator + 1);

        // Keep the original Math-owned cache location.
        cacheFolder = executableFolder + L"Cache\\";
        if (!Folder(cacheFolder))
            return false;
        cacheFolder += L"SAO_Toth_Minimized_Overlay_Math\\";
        if (!Folder(cacheFolder))
            return false;

        // This PC > Desktop is an Explorer location, not a literal file-system path.
        // Locate assets from the running game's project folder instead.
        std::wstring parent = executableFolder;
        for (int level = 0; level < 6; ++level)
        {
            const std::wstring candidate = parent +
                L"SAO_I_E Assets\\Toth D.I\\Minimized Overlay\\";
            const DWORD attributes = GetFileAttributesW(candidate.c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                pngFolder = candidate;
                return true;
            }

            // Move from Debug -> project (or x64/Debug -> x64 -> project).
            if (!parent.empty() && (parent.back() == L'\\' || parent.back() == L'/'))
                parent.pop_back();
            const std::size_t previous = parent.find_last_of(L"\\/");
            if (previous == std::wstring::npos)
                break;
            parent.resize(previous + 1);
        }

        return false;
    }

    bool Fresh(const std::wstring& source, const std::wstring& cached, UINT bytes)
    {
        WIN32_FILE_ATTRIBUTE_DATA png{};
        WIN32_FILE_ATTRIBUTE_DATA bitmap{};

        return GetFileAttributesExW(source.c_str(), GetFileExInfoStandard, &png) &&
            !(png.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            GetFileAttributesExW(cached.c_str(), GetFileExInfoStandard, &bitmap) &&
            !(bitmap.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            bitmap.nFileSizeHigh == 0 &&
            bitmap.nFileSizeLow == bytes &&
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

        if (written && closed &&
            MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return true;

        DeleteFileW(temporary.c_str());
        return false;
    }

    // Existing 1920x1080 bitmap caches are reused without decoding PNGs.
    // The original overlay conductor requires every asset on this canvas.
    constexpr UINT CANVAS_WIDTH = 1920;
    constexpr UINT CANVAS_HEIGHT = 1080;
    constexpr UINT CANVAS_BYTES = CANVAS_WIDTH * CANVAS_HEIGHT * 4;

    bool ConvertOne(IWICImagingFactory* factory,
        const std::wstring& png,
        SAO_Toth_Minimized_Overlay_Math::BitmapCacheNode& node,
        std::vector<BYTE>& pixels,
        std::wstring& error)
    {
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;

        HRESULT hr = factory->CreateDecoderFromFilename(
            png.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);

        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);

        UINT width = 0;
        UINT height = 0;

        if (SUCCEEDED(hr)) hr = frame->GetSize(&width, &height);

        if (SUCCEEDED(hr))
        {
            if (!width || !height || width > UINT_MAX / 4 || height > UINT_MAX / (width * 4))
                hr = E_FAIL;
        }

        if (SUCCEEDED(hr) && (width != CANVAS_WIDTH || height != CANVAS_HEIGHT))
            hr = E_INVALIDARG;

        if (SUCCEEDED(hr))
        {
            node.width = CANVAS_WIDTH;
            node.height = CANVAS_HEIGHT;
            node.bytes = CANVAS_BYTES;
            {
                hr = factory->CreateFormatConverter(&converter);

                if (SUCCEEDED(hr))
                {
                    hr = converter->Initialize(
                        frame,
                        GUID_WICPixelFormat32bppPBGRA,
                        WICBitmapDitherTypeNone,
                        nullptr,
                        0.0,
                        WICBitmapPaletteTypeCustom);
                }

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

        if (FAILED(hr))
            error = L"Cannot decode or cache PNG: " + png;

        return SUCCEEDED(hr);
    }
}

const SAO_Toth_Minimized_Overlay_Math::OutputNode&
SAO_Toth_Minimized_Overlay_Math::Build()
{
    if (Cache.ready) return Cache;
    Cache = OutputNode{};

    std::wstring pngFolder;
    std::wstring cacheFolder;

    if (!Locations(pngFolder, cacheFolder))
    {
        Cache.error = L"Cannot locate Toth Minimized Overlay PNGs or create the cache folder.";
        return Cache;
    }

    // No COM or WIC decoder is constructed when the source caches are fresh.
    // One temporary bitmap buffer is allocated only when a PNG changed.
    HRESULT com = RPC_E_CHANGED_MODE;
    IWICImagingFactory* factory = nullptr;
    std::vector<BYTE> scratch;
    bool all = true;

    for (std::size_t i = 0; i < COUNT; ++i)
    {
        BitmapCacheNode& node = Cache.assets[i];
        node.name = PNG_NAMES[i];
        node.path = cacheFolder + node.name + L".cache";
        const std::wstring png = pngFolder + node.name;

        if (Fresh(png, node.path, CANVAS_BYTES))
        {
            node.width = CANVAS_WIDTH;
            node.height = CANVAS_HEIGHT;
            node.bytes = CANVAS_BYTES;
            node.ready = true;
            continue;
        }

        if (!factory)
        {
            com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            if (FAILED(com) && com != RPC_E_CHANGED_MODE)
            {
                Cache.error = L"Windows COM initialization failed.";
                all = false;
                break;
            }
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
            {
                Cache.error = L"Windows WIC bitmap decoder creation failed.";
                all = false;
                break;
            }
        }
        if (!ConvertOne(factory, png, node, scratch, Cache.error))
        {
            all = false;
            break;
        }
    }
    if (factory) factory->Release();
    if (com == S_OK || com == S_FALSE) CoUninitialize();
    // Explicit scratch release: only tiny path/metadata nodes survive.
    std::vector<BYTE>().swap(scratch);
    Cache.ready = all;
    return Cache;
}