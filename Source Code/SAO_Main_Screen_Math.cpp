#include <windows.h>
#include <initguid.h>
#include <wincodec.h>
#include <filesystem>
#include <cstdio>
#include <vector>
#include "SAO_Main_Screen_Math.h"
#include <process.h>
#include "SAO_Main_Screen_Math_Cache_Node.h"

#pragma comment(lib, "ole32.lib")

SAO_Main_Screen_Math::OutputNode SAO_Main_Screen_Math::Cache{};

namespace
{
    constexpr UINT WIDTH = 1920, HEIGHT = 1080;
    constexpr UINT BYTES = WIDTH * HEIGHT * 4;
    constexpr const wchar_t* NAMES[] = {
        L"0001.png",
        L"0002.png",
        L"0003.png",
        L"0004.png",
        L"0005.png",
        L"0006.png",
        L"0007.png",
        L"0008.png",
        L"0009.png",
        L"0010.png",
        L"0011.png",
        L"0012.png",
        L"0013.png",
        L"0014.png",
        L"0015.png",
        L"0016.png",
        L"0017.png",
        L"0018.png",
        L"0019.png",
        L"0020.png",
        L"0021.png",
        L"0022.png",
        L"0023.png",
        L"0024.png",
        L"SAO_Main_Frame.png"
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

const SAO_Main_Screen_Math::OutputNode& SAO_Main_Screen_Math::Build(bool preload)
{

    SAO_Main_Screen_Math::Cache.ready = false;
    SAO_Main_Screen_Math::Cache.error.clear();

    wchar_t executable[MAX_PATH]{};
    DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
    if (!length || length >= MAX_PATH)
    {
        SAO_Main_Screen_Math::Cache.error = L"Cannot locate the executable.";
        return SAO_Main_Screen_Math::Cache;
    }
    std::wstring folder(executable, length);
    folder.resize(folder.find_last_of(L"\\/") + 1);
    std::wstring cacheFolder = folder + L"Cache\\";
    if (!CreateDirectoryW(cacheFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SAO_Main_Screen_Math::Cache.error = L"Cannot create: " + cacheFolder;
        return SAO_Main_Screen_Math::Cache;
    }

    cacheFolder += L"SAO_Main_Screen_Math\\";
    if (!CreateDirectoryW(cacheFolder.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        SAO_Main_Screen_Math::Cache.error = L"Cannot create: " + cacheFolder;
        return SAO_Main_Screen_Math::Cache;
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
        SAO_Main_Screen_Math::Cache.error = L"Cannot locate SAO_I_E Assets beside the executable or in its parent folders.";
        return SAO_Main_Screen_Math::Cache;
    }
    const std::wstring sourceFolder = (assets / LR"(Main Screen\)").wstring();

    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE)
    {
        SAO_Main_Screen_Math::Cache.error = L"Cannot initialize Windows PNG decoding.";
        return SAO_Main_Screen_Math::Cache;
    }
    IWICImagingFactory* factory = nullptr;
    bool success = true;
    std::vector<BYTE> pixels;

    for (unsigned i = 0; i < COUNT && success; ++i)
    {
        if (!preload && i != 0 && i != 24) continue;
        if (SAO_Main_Screen_Math::Cancelled()) { success = false; break; }
        auto& cache = SAO_Main_Screen_Math::Cache.layers[i];
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
        if (!success) { SAO_Main_Screen_Math::Cache.error = L"PNG decode failed (1920x1080 required): " + png; break; }
        FILE* output = nullptr;
        success = _wfopen_s(&output, cache.path.c_str(), L"wb") == 0 && output;
        if (success)
        {
            const bool written = fwrite(pixels.data(), 1, BYTES, output) == BYTES;
            success = fclose(output) == 0 && written;
            if (!success) DeleteFileW(cache.path.c_str());
        }
        if (!success) SAO_Main_Screen_Math::Cache.error = L"Cache write failed: " + cache.path;
        cache.ready = success;
    }
    if (factory) factory->Release();
    if (SUCCEEDED(com)) CoUninitialize();
    SAO_Main_Screen_Math::Cache.ready = success;
    if (!success && SAO_Main_Screen_Math::Cache.error.empty())
        SAO_Main_Screen_Math::Cache.error = L"Cannot create the Windows PNG decoder.";
    return SAO_Main_Screen_Math::Cache;
}

namespace
{
    HANDLE source_worker = nullptr, preload_event = nullptr, cancel_event = nullptr;
}

bool SAO_Main_Screen_Math::Cancelled()
{
    return cancel_event && WaitForSingleObject(cancel_event, 0) == WAIT_OBJECT_0;
}

static unsigned __stdcall Emit(void* target)
{
    const HWND window = static_cast<HWND>(target);
    // Only the first image and static artwork are needed before the first paint.
    const auto& screen = SAO_Main_Screen_Math::Build(false);
    const auto& particles = SAO_Main_Screen_Particle_Math::Build(false);
    const auto& title = SAO_Main_Title_Math::Build();
    const auto& buttons = SAO_Main_Buttons_Math::Build();
    if (SAO_Main_Screen_Math::Cancelled()) return 0;
    SAO_Main_Screen_Math_Cache_Node::Build(screen, particles, title, buttons, window);
    if (!screen.ready || !particles.ready || !title.ready || !buttons.ready) return 0;

    const HANDLE events[] = { cancel_event, preload_event };
    if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1) return 0;
    SAO_Main_Screen_Math::Build(true);
    SAO_Main_Screen_Particle_Math::Build(true);
    if (!SAO_Main_Screen_Math::Cancelled())
        SAO_Main_Screen_Math_Cache_Node::Build(screen, particles, title, buttons, window);
    return 0;
}

bool SAO_Main_Screen_Math::Start(HWND target)
{
    if (source_worker) return true;
    cancel_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    preload_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (cancel_event && preload_event)
        source_worker = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, Emit, target, 0, nullptr));
    if (source_worker) return true;
    if (preload_event) CloseHandle(preload_event);
    if (cancel_event) CloseHandle(cancel_event);
    preload_event = cancel_event = nullptr;
    return false;
}

void SAO_Main_Screen_Math::Preload()
{
    if (preload_event) SetEvent(preload_event);
}

void SAO_Main_Screen_Math::Stop()
{
    if (cancel_event) SetEvent(cancel_event);
    if (source_worker)
    {
        WaitForSingleObject(source_worker, INFINITE);
        CloseHandle(source_worker);
    }
    if (preload_event) CloseHandle(preload_event);
    if (cancel_event) CloseHandle(cancel_event);
    source_worker = preload_event = cancel_event = nullptr;
}

