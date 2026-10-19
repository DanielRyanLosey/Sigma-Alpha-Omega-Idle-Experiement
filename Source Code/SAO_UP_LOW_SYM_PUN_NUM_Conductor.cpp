#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_UP_LOW_SYM_PUN_NUM_Conductor.h"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>


namespace SAO_UP_LOW_SYM_PUN_NUM_Conductor
{
    OutputNode Cache{};

    namespace
    {
        constexpr std::size_t COUNT = 110; // 26 + 26 + 16 + 32 + 10
        constexpr wchar_t PUNCTUATION[32] =
        {
            L'&', L'\'', L'*', L'@', L'\\', L'^', L':', L',', L'$', L'"',
            L'=', L'!', L'`', L'>', L'#', L'-', L'{', L'[', L'(', L'<',
            L'%', L'.', L'+', L'?', L'}', L']', L')', L';', L'/', L'~',
            L'_', L'|'
        };
        constexpr wchar_t SYMBOLS[16] =
        {
            L'\u03B1', L'\u00B0', L'\u0394', L'\u00F7',
            L'\u2265', L'\u221E', L'\u2264', L'\u00D7',
            L'\u2260', L'\u03C9', L'\u03A9', L'\u03C0',
            L'\u00B1', L'\u03A3', L'\u221A', L'\u03B8'
        };

        struct SourceNode
        {
            const std::wstring* path = nullptr;
            std::uint32_t width = 0, height = 0, bytes = 0;
            wchar_t character = 0;
        };

        std::array<std::array<SourceNode, COUNT>, 3> sources{};
        std::wstring directory;

        bool Folder(const std::wstring& path)
        {
            if (CreateDirectoryW(path.c_str(), nullptr)) return true;
            return GetLastError() == ERROR_ALREADY_EXISTS;
        }

        bool Locations()
        {
            wchar_t executable[MAX_PATH]{};
            const DWORD count = GetModuleFileNameW(nullptr, executable, MAX_PATH);
            if (!count || count >= MAX_PATH) return false;
            std::wstring path(executable, count);
            const std::size_t separator = path.find_last_of(L"\\/");
            if (separator == std::wstring::npos) return false;
            path.resize(separator + 1);
            path += L"Cache\\";
            if (!Folder(path)) return false;
            path += L"SAO_UP_LOW_SYM_PUN_NUM_Conductor\\";
            if (!Folder(path)) return false;
            directory = path;
            return true;
        }

        bool Stamp(const std::wstring& path, std::uint64_t bytes, FILETIME* modified)
        {
            WIN32_FILE_ATTRIBUTE_DATA data{};
            if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data) ||
                (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
            const std::uint64_t length =
                (std::uint64_t(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
            if (length != bytes) return false;
            if (modified) *modified = data.ftLastWriteTime;
            return true;
        }

        bool Fresh(const std::wstring& source, const std::wstring& cached,
            std::uint64_t expectedBytes)
        {
            FILETIME a{}, b{};
            return Stamp(source, expectedBytes, &a) &&
                Stamp(cached, expectedBytes, &b) && CompareFileTime(&b, &a) >= 0;
        }

        bool SaveMove(const std::wstring& temporary, const std::wstring& target)
        {
            if (MoveFileExW(temporary.c_str(), target.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
            DeleteFileW(temporary.c_str());
            return false;
        }

        // Source paths remain private. Every returned cache is owned here.
        template<class Node>
        void TakePointer(const Node& node, std::size_t size,
            std::size_t index, wchar_t character)
        {
            sources[size][index] =
            { node.path, node.width, node.height, node.bytes, character };
        }

        bool Connect(const SAO_Upper_Conductor::OutputNode& upper,
            const SAO_Lower_Conductor::OutputNode& lower,
            const SAO_Symbol_Conductor::OutputNode& symbol,
            const SAO_Punctuation_Conductor::OutputNode& punctuation,
            const SAO_Numbers_Conductor::OutputNode& numbers)
        {
            if (!upper.ready || !lower.ready || !symbol.ready ||
                !punctuation.ready || !numbers.ready)
            {
                Cache.error = !upper.ready ? upper.error :
                    !lower.ready ? lower.error : !symbol.ready ? symbol.error :
                    !punctuation.ready ? punctuation.error : numbers.error;
                if (Cache.error.empty()) Cache.error = L"One upstream conductor is not ready.";
                return false;
            }
            for (std::size_t size = 0; size < 3; ++size)
            {
                for (std::size_t i = 0; i < 26; ++i)
                {
                    TakePointer(upper.letters[size][i], size, i, wchar_t(L'A' + i));
                    TakePointer(lower.letters[size][i], size, 26 + i, wchar_t(L'a' + i));
                }
                for (std::size_t i = 0; i < 16; ++i)
                    TakePointer(symbol.symbols[size][i], size, 52 + i, SYMBOLS[i]);
                for (std::size_t i = 0; i < 32; ++i)
                    TakePointer(punctuation.symbols[size][i], size, 68 + i, PUNCTUATION[i]);
                for (std::size_t i = 0; i < 10; ++i)
                    TakePointer(numbers.numbers[size][i], size, 100 + i, wchar_t(L'0' + i));
            }
            for (const auto& family : sources)
            {
                for (const auto& src : family)
                {
                    if (!src.path || src.path->empty() || !src.width || !src.height ||
                        std::uint64_t(src.width) * src.height * 4 != src.bytes ||
                        !Stamp(*src.path, src.bytes, nullptr))
                    {
                        Cache.error = L"An upstream bitmap cache is missing or has an invalid size.";
                        return false;
                    }
                }
            }
            return true;
        }

        int Index(wchar_t character)
        {
            if (character >= L'A' && character <= L'Z') return int(character - L'A');
            if (character >= L'a' && character <= L'z') return 26 + int(character - L'a');
            if (character >= L'0' && character <= L'9') return 100 + int(character - L'0');
            for (int i = 0; i < 16; ++i)
                if (SYMBOLS[i] == character) return 52 + i;
            for (int i = 0; i < 32; ++i)
                if (PUNCTUATION[i] == character) return 68 + i;
            return -1;
        }

        wchar_t SizeCode(Size size)
        {
            switch (size)
            {
            case Size::Small: return L'S';
            case Size::Medium: return L'M';
            case Size::Large: return L'L';
            default: return 0;
            }
        }

        // Stable identifier only; exact UTF-16 request is also checked by metadata.
        std::uint64_t RequestId(Size size, Orientation orientation,
            const std::wstring& text)
        {
            std::uint64_t value = 14695981039346656037ull;
            const auto add = [&value](std::uint16_t x) {
                value ^= std::uint8_t(x & 0xff); value *= 1099511628211ull;
                value ^= std::uint8_t(x >> 8); value *= 1099511628211ull;
            };
            add(static_cast<std::uint16_t>(size));
            add(static_cast<std::uint16_t>(orientation));
            for (const wchar_t c : text) add(static_cast<std::uint16_t>(c));
            return value;
        }

        bool MetadataMatches(const std::wstring& path, Size size,
            Orientation orientation, const std::wstring& text)
        {
            FILE* file = nullptr;
            if (_wfopen_s(&file, path.c_str(), L"rb") != 0 || !file) return false;
            std::uint32_t count = 0;
            std::uint16_t flags[2]{};
            bool good = fread(&count, sizeof(count), 1, file) == 1 &&
                fread(flags, sizeof(flags), 1, file) == 1 &&
                count == text.size() && flags[0] == std::uint16_t(size) &&
                flags[1] == std::uint16_t(orientation);
            for (wchar_t c : text)
            {
                std::uint16_t stored = 0;
                if (!good || fread(&stored, sizeof(stored), 1, file) != 1 ||
                    stored != std::uint16_t(c)) {
                    good = false; break;
                }
            }
            if (good && fgetc(file) != EOF) good = false;
            fclose(file);
            return good;
        }

        bool WriteMetadata(const std::wstring& path, Size size,
            Orientation orientation, const std::wstring& text)
        {
            const std::wstring temp = path + L".tmp";
            FILE* file = nullptr;
            if (_wfopen_s(&file, temp.c_str(), L"wb") != 0 || !file) return false;
            const std::uint32_t count = static_cast<std::uint32_t>(text.size());
            const std::uint16_t flags[2] = {
                std::uint16_t(size), std::uint16_t(orientation) };
            bool good = fwrite(&count, sizeof(count), 1, file) == 1 &&
                fwrite(flags, sizeof(flags), 1, file) == 1;
            for (wchar_t c : text)
            {
                const std::uint16_t u = static_cast<std::uint16_t>(c);
                if (good) good = fwrite(&u, sizeof(u), 1, file) == 1;
            }
            const bool closed = fclose(file) == 0;
            if (good && closed) return SaveMove(temp, path);
            DeleteFileW(temp.c_str());
            return false;
        }


    }

    const OutputNode& Build(const SAO_Upper_Conductor::OutputNode& upper,
        const SAO_Lower_Conductor::OutputNode& lower,
        const SAO_Symbol_Conductor::OutputNode& symbols,
        const SAO_Punctuation_Conductor::OutputNode& punctuation,
        const SAO_Numbers_Conductor::OutputNode& numbers)
    {
        Cache = OutputNode{};
        sources = {};
        directory.clear();
        if (!Locations())
        {
            Cache.error = L"Cannot create this conductor's own cache directory.";
            return Cache;
        }
        Cache.ready = Connect(upper, lower, symbols, punctuation, numbers);
        return Cache;
    }

    BitmapCacheNode Character(Size size, wchar_t character)
    {
        BitmapCacheNode result{};
        if (!Cache.ready || !SizeCode(size)) return result;
        const int index = Index(character);
        if (index < 0) { Cache.error = L"Character has no supplied bitmap asset."; return result; }
        const SourceNode& src = sources[std::size_t(size)][std::size_t(index)];
        result.name = L"SAO_Char_" + std::to_wstring(std::uint16_t(character)) +
            L"_" + SizeCode(size);
        result.path = directory + result.name + L".cache";
        result.width = src.width;
        result.height = src.height;
        result.bytes = src.bytes;
        if (!Fresh(*src.path, result.path, result.bytes))
        {
            const std::wstring temporary = result.path + L".tmp";
            if (!CopyFileW(src.path->c_str(), temporary.c_str(), FALSE) ||
                !SaveMove(temporary, result.path))
            {
                DeleteFileW(temporary.c_str());
                Cache.error = L"Cannot re-cache character bitmap: " + result.name;
                return result;
            }
        }
        result.ready = Stamp(result.path, result.bytes, nullptr);
        return result;
    }

    BitmapCacheNode Orient(Size size, const std::wstring& characters,
        Orientation orientation)
    {
        BitmapCacheNode result{};
        result.orientation = orientation;
        if (!Cache.ready || !SizeCode(size) || characters.empty() ||
            characters.size() > 4096 || (orientation != Orientation::Left &&
                orientation != Orientation::Right)) return result;

        std::vector<BitmapCacheNode> tiles;
        tiles.reserve(characters.size());
        std::uint64_t width = 0, maximumHeight = 0;
        FILETIME latest{};
        for (wchar_t c : characters)
        {
            BitmapCacheNode glyph = Character(size, c);
            if (!glyph.ready) return result;
            width += glyph.width;
            if (width > (std::numeric_limits<std::uint32_t>::max)() / 4) return result;
            if (glyph.height > maximumHeight) maximumHeight = glyph.height;
            FILETIME stamp{};
            if (!Stamp(glyph.path, glyph.bytes, &stamp)) return result;
            if (CompareFileTime(&stamp, &latest) > 0) latest = stamp;
            tiles.push_back(std::move(glyph));
        }
        if (!width || !maximumHeight ||
            maximumHeight > (std::numeric_limits<std::uint64_t>::max)() / (width * 4))
            return result;
        const std::uint64_t total = width * maximumHeight * 4;
        result.width = static_cast<std::uint32_t>(width);
        result.height = static_cast<std::uint32_t>(maximumHeight);
        result.bytes = total;

        // Every request is verified by exact metadata, even if a filename hash collides.
        const std::uint64_t key = RequestId(size, orientation, characters);
        const std::wstring prefix = directory + L"SAO_Oriented_" +
            std::to_wstring(key);
        for (std::uint32_t collision = 0; collision < 1024; ++collision)
        {
            const std::wstring stem = prefix + L"_" + std::to_wstring(collision);
            if (!MetadataMatches(stem + L".meta", size, orientation, characters) &&
                GetFileAttributesW((stem + L".meta").c_str()) != INVALID_FILE_ATTRIBUTES)
                continue;
            result.name = stem.substr(directory.size());
            result.path = stem + L".cache";
            const std::wstring meta = stem + L".meta";
            if (MetadataMatches(meta, size, orientation, characters))
            {
                FILETIME written{};
                if (Stamp(result.path, result.bytes, &written) &&
                    CompareFileTime(&written, &latest) >= 0)
                {
                    result.ready = true;
                    return result;
                }
            }
            break;
        }
        if (result.path.empty()) { Cache.error = L"Cache filename collision limit reached."; return result; }

        const std::wstring temporary = result.path + L".tmp";
        FILE* destination = nullptr;
        if (_wfopen_s(&destination, temporary.c_str(), L"wb") != 0 || !destination)
        {
            Cache.error = L"Cannot open output bitmap cache."; return result;
        }

        std::vector<FILE*> files(tiles.size(), nullptr);
        bool success = true;
        for (std::size_t i = 0; i < tiles.size(); ++i)
        {
            if (_wfopen_s(&files[i], tiles[i].path.c_str(), L"rb") != 0 || !files[i])
            {
                success = false; break;
            }
        }
        std::vector<std::uint8_t> row;
        if (success) row.resize(std::size_t(width) * 4);
        for (std::uint32_t y = 0; success && y < maximumHeight; ++y)
        {
            std::size_t offset = 0;
            for (std::size_t step = 0; step < tiles.size(); ++step)
            {
                const std::size_t i = orientation == Orientation::Left ?
                    step : tiles.size() - step - 1;
                const auto& tile = tiles[i];
                const std::size_t stride = std::size_t(tile.width) * 4;
                if (y >= tile.height)
                    std::memset(row.data() + offset, 0, stride);
                else if (fread(row.data() + offset, 1, stride, files[i]) != stride)
                {
                    success = false; break;
                }
                offset += stride;
            }
            if (success && fwrite(row.data(), 1, row.size(), destination) != row.size())
                success = false;
        }
        for (FILE* f : files) if (f) fclose(f);
        if (fclose(destination) != 0) success = false;
        if (success) success = SaveMove(temporary, result.path);
        else DeleteFileW(temporary.c_str());
        if (success) success = WriteMetadata(result.path.substr(0, result.path.size() - 6) +
            L".meta", size, orientation, characters);
        result.ready = success && Stamp(result.path, result.bytes, nullptr);
        if (!result.ready) Cache.error = L"Cannot write oriented bitmap cache.";
        return result;
    }

    BitmapCacheNode Orient(Size size, wchar_t left, wchar_t right,
        Orientation orientation)
    {
        const std::wstring pair{ left, right };
        return Orient(size, pair, orientation);
    }
}
