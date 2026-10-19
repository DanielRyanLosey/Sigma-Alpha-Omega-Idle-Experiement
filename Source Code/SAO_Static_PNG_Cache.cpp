#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_Static_PNG_Cache.h"
#include <wincodec.h>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdio>
#pragma comment(lib,"ole32.lib")
#pragma comment(lib,"windowscodecs.lib")

namespace SAO_Static_PNG_Cache {
    namespace fs = std::filesystem;
    bool Cache(const std::wstring& sourceFolder, const std::wstring& cacheFolder,
        BitmapNode& node) {
        if (node.ready) return true;
        const fs::path source = fs::path(sourceFolder) / node.name;
        const fs::path folder(cacheFolder);
        std::error_code ec;
        fs::create_directories(folder, ec);
        if (ec) return false;
        const fs::path target = folder / (node.name + L".cache");
        // Small sidecar stores exact decoded width/height, avoiding guessed placement.
        const fs::path dims = folder / (node.name + L".size");
        auto stamp = fs::last_write_time(source, ec);
        if (ec) return false;
        bool valid = fs::exists(target, ec) && fs::exists(dims, ec) &&
            fs::last_write_time(target, ec) >= stamp;
        if (valid) {
            std::ifstream in(dims, std::ios::binary);
            std::uint32_t w = 0, h = 0; in.read(reinterpret_cast<char*>(&w), 4);
            in.read(reinterpret_cast<char*>(&h), 4);
            if (in && w > 0 && h > 0 && std::uint64_t(w) * h * 4 == fs::file_size(target, ec)) {
                node.width = w; node.height = h; node.path = target.wstring();
                return node.ready = true;
            }
        }
        HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
        IWICImagingFactory* factory = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
        if (SUCCEEDED(hr)) hr = factory->CreateDecoderFromFilename(source.c_str(), nullptr,
            GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
        if (SUCCEEDED(hr)) hr = decoder->GetFrame(0, &frame);
        UINT w = 0, h = 0;
        if (SUCCEEDED(hr)) hr = frame->GetSize(&w, &h);
        if (SUCCEEDED(hr) && (!w || !h || w > 4096 || h > 4096)) hr = E_FAIL;
        if (SUCCEEDED(hr)) hr = factory->CreateFormatConverter(&converter);
        if (SUCCEEDED(hr)) hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom);
        const fs::path tmp = target.wstring() + L".tmp";
        std::ofstream out;
        if (SUCCEEDED(hr)) {
            out.open(tmp, std::ios::binary | std::ios::trunc);
            if (!out)hr = E_FAIL;
        }
        if (SUCCEEDED(hr)) {
            std::vector<BYTE> row(std::size_t(w) * 4);
            for (UINT y = 0; y < h && SUCCEEDED(hr); ++y) {
                WICRect rect{ 0,static_cast<INT>(y),static_cast<INT>(w),1 };
                hr = converter->CopyPixels(&rect, w * 4, w * 4, row.data());
                if (SUCCEEDED(hr)) {
                    out.write(reinterpret_cast<const char*>(row.data()), row.size());
                    if (!out)hr = E_FAIL;
                }
            }
        }
        out.close();
        if (converter)converter->Release();
        if (frame)frame->Release();
        if (decoder)decoder->Release();
        if (factory)factory->Release();
        if (SUCCEEDED(initialized))CoUninitialize();
        if (FAILED(hr)) { fs::remove(tmp, ec); return false; }
        fs::rename(tmp, target, ec);
        if (ec) { fs::remove(target, ec); ec.clear(); fs::rename(tmp, target, ec); }
        if (ec)return false;
        std::ofstream sizefile(dims, std::ios::binary | std::ios::trunc);
        sizefile.write(reinterpret_cast<const char*>(&w), 4);
        sizefile.write(reinterpret_cast<const char*>(&h), 4);
        if (!sizefile)return false;
        node.width = w; node.height = h; node.path = target.wstring();
        return node.ready = true;
    }
    bool Load(const BitmapNode& node, std::vector<BYTE>& pixels) {
        if (!node.ready)return false;
        const std::uint64_t count = std::uint64_t(node.width) * node.height * 4;
        if (!count || count > UINT32_MAX)return false;
        std::ifstream in(fs::path(node.path), std::ios::binary);
        if (!in)return false;
        pixels.resize(static_cast<std::size_t>(count));
        return bool(in.read(reinterpret_cast<char*>(pixels.data()), pixels.size()));
    }
    Anchor Locate(const BitmapNode& node) {
        Anchor answer{};
        if (!node.ready)return answer;
        std::ifstream in(fs::path(node.path), std::ios::binary);
        if (!in)return answer;
        std::vector<BYTE> row(std::size_t(node.width) * 4);
        std::uint64_t sumx = 0, sumy = 0, count = 0;
        for (UINT y = 0; y < node.height; ++y) {
            if (!in.read(reinterpret_cast<char*>(row.data()), row.size()))return answer;
            for (UINT x = 0; x < node.width; ++x) {
                if (row[std::size_t(x) * 4 + 3] > 24) { sumx += x; sumy += y; ++count; }
            }
        }
        if (count) { answer.x = int(sumx / count); answer.y = int(sumy / count); answer.ready = true; }
        return answer;
    }
}