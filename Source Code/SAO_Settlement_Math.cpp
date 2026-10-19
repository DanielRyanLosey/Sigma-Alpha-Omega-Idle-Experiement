#include "SAO_Settlement_Math.h"
#include "SAO_Settlement_Conductor.h"
#include <wincodec.h>
#include <process.h>
#include <cstdio>
#include <filesystem>

#pragma comment(lib, "ole32.lib")

namespace SAO_Settlement_Math
{
    OutputNode Cache{};
    namespace
    {
        constexpr UINT WIDTH = 1920, HEIGHT = 1080;
        constexpr UINT STRIDE = WIDTH * 4, BYTES = STRIDE * HEIGHT;
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
                    return (assets / LR"(Settlement\)").wstring();
                auto parent = root.parent_path();
                if (parent == root) break;
                root = parent;
            }
            return {};
        }

        HANDLE worker = nullptr, preloadEvent = nullptr, cancelEvent = nullptr;

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
            folder += L"SAO_Settlement_Math\\";
            return CreateDirectoryW(folder.c_str(), nullptr) ||
                GetLastError() == ERROR_ALREADY_EXISTS;
        }

        bool Valid(const std::wstring& png, const std::wstring& cache)
        {
            WIN32_FILE_ATTRIBUTE_DATA source{}, saved{};
            return GetFileAttributesExW(png.c_str(), GetFileExInfoStandard, &source) &&
                GetFileAttributesExW(cache.c_str(), GetFileExInfoStandard, &saved) &&
                !(saved.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                !saved.nFileSizeHigh && saved.nFileSizeLow == BYTES &&
                CompareFileTime(&saved.ftLastWriteTime, &source.ftLastWriteTime) >= 0;
        }

        bool Bitmap(IWICImagingFactory* factory,
            const std::wstring& png, const std::wstring& output)
        {
            IWICBitmapDecoder* decoder = nullptr;
            IWICBitmapFrameDecode* frame = nullptr;
            IWICFormatConverter* converter = nullptr;
            HRESULT result = factory->CreateDecoderFromFilename(png.c_str(), nullptr,
                GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
            if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
            UINT width = 0, height = 0;
            if (SUCCEEDED(result)) result = frame->GetSize(&width, &height);
            if (SUCCEEDED(result) && (width != WIDTH || height != HEIGHT)) result = E_FAIL;
            if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
            if (SUCCEEDED(result)) result = converter->Initialize(frame,
                GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
                nullptr, 0, WICBitmapPaletteTypeCustom);
            const std::wstring temporary = output + L".tmp";
            FILE* file = nullptr;
            if (SUCCEEDED(result) &&
                (_wfopen_s(&file, temporary.c_str(), L"wb") != 0 || !file)) result = E_FAIL;
            BYTE row[STRIDE];
            for (UINT y = 0; SUCCEEDED(result) && y < HEIGHT; ++y)
            {
                if (Cancelled()) { result = E_ABORT; break; }
                WICRect rectangle{ 0, static_cast<INT>(y), static_cast<INT>(WIDTH), 1 };
                result = converter->CopyPixels(&rectangle, STRIDE, STRIDE, row);
                if (SUCCEEDED(result) && fwrite(row, 1, STRIDE, file) != STRIDE)
                    result = E_FAIL;
            }
            if (file && fclose(file) != 0) result = E_FAIL;
            if (converter) converter->Release();
            if (frame) frame->Release();
            if (decoder) decoder->Release();
            if (SUCCEEDED(result) && MoveFileExW(temporary.c_str(), output.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temporary.c_str());
            return false;
        }

        bool CacheOne(IWICImagingFactory*& factory, BitmapNode& node,
            const std::wstring& folder, const std::wstring& source)
        {
            if (node.ready) return true;
            node.path = folder + node.name + L".cache";
            const std::wstring png = source + node.name;
            if (Valid(png, node.path)) return node.ready = true;
            if (!factory && FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
                CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory)))) return false;
            return node.ready = Bitmap(factory, png, node.path);
        }

        unsigned __stdcall Emit(void* context)
        {
            const HWND target = static_cast<HWND>(context);
            const auto& first = Build(false);
            if (Cancelled()) return 0;
            SAO_Settlement_Conductor::Build(first, target);
            // Preload can still prepare the branch if the first cache was not ready.
            const HANDLE events[]{ cancelEvent, preloadEvent };
            if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
                return 0;
            const auto& all = Build(true);
            if (!Cancelled()) SAO_Settlement_Conductor::Build(all, target);
            return 0;
        }
    }

    bool Cancelled()
    {
        return cancelEvent && WaitForSingleObject(cancelEvent, 0) == WAIT_OBJECT_0;
    }

    const OutputNode& Build(bool preload)
    {
        Cache.error.clear();
        Cache.ready = false;
        const std::wstring source = SourceFolder();
        if (source.empty())
        {
            Cache.error = L"Settlement asset folder unavailable beside the executable or in its parent folders.";
            return Cache;
        }
        std::wstring folder;
        if (!Directory(folder))
        {
            Cache.error = L"Settlement Math cache folder could not be created.";
            return Cache;
        }
        const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(com) && com != RPC_E_CHANGED_MODE)
        {
            Cache.error = L"Settlement PNG decoder initialization failed.";
            return Cache;
        }
        IWICImagingFactory* factory = nullptr;
        bool success = true;
        for (UINT i = 0; i < COUNT && success; ++i)
        {
            if (!preload && i) continue;
            wchar_t name[64]{};
            swprintf_s(name, L"SAO_Settlement_%04u.png", i + 1);
            Cache.scenes[i].name = name;
            success = !Cancelled() && CacheOne(factory, Cache.scenes[i], folder, source);
            if (!success) Cache.error = L"Settlement PNG/cache failed: " +
                source + name + L" (1920x1080 required).";
        }
        Cache.harvesting.name = L"SAO_Settlement_Harvesting_Button.png";
        if (success) success = !Cancelled() && CacheOne(factory, Cache.harvesting, folder, source);
        if (!success && Cache.error.empty()) Cache.error =
            L"Settlement harvesting PNG/cache failed: " +
            source + Cache.harvesting.name + L" (1920x1080 required).";
        Cache.back.name = L"SAO_Settlement_Back.png";
        if (success) success = !Cancelled() && CacheOne(factory, Cache.back, folder, source);
        if (!success && Cache.error.empty()) Cache.error =
            L"Settlement Back PNG/cache failed: " +
            source + Cache.back.name + L" (1920x1080 required).";
        if (factory) factory->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        Cache.ready = success;
        if (preload) Cache.complete.store(true, std::memory_order_release);
        return Cache;
    }

    bool Start(HWND target)
    {
        if (worker) return true;
        cancelEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        preloadEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (cancelEvent && preloadEvent)
            worker = reinterpret_cast<HANDLE>(
                _beginthreadex(nullptr, 0, Emit, target, 0, nullptr));
        if (worker) return true;
        if (cancelEvent) CloseHandle(cancelEvent);
        if (preloadEvent) CloseHandle(preloadEvent);
        cancelEvent = preloadEvent = nullptr;
        return false;
    }

    void Preload() { if (preloadEvent) SetEvent(preloadEvent); }

    void Stop()
    {
        if (cancelEvent) SetEvent(cancelEvent);
        if (worker) { WaitForSingleObject(worker, INFINITE); CloseHandle(worker); }
        if (cancelEvent) CloseHandle(cancelEvent);
        if (preloadEvent) CloseHandle(preloadEvent);
        worker = cancelEvent = preloadEvent = nullptr;
    }
}