#include "SAO_English_Rules_Math.h"
#include <cstdio>
#include <cwchar>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace SAO_English_Rules_Math
{
    OutputNode Cache{};

    namespace
    {
        // The only runtime rule data: fixed eight-byte descriptors.
        // Full prose explanations are kept in SAO_English_Rules_REFERENCE.txt,
        // NOT copied into the runtime cache.
        constexpr std::array<RuleNode, RULE_COUNT> RULES = { {
        { 1, Category::Sentence, Mode::Foundation, Action::Recognize, 1, 0, 0 },
        { 2, Category::Sentence, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 3, Category::Sentence, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 4, Category::Sentence, Mode::Foundation, Action::Separate, 1, 0, 0 },
        { 5, Category::Sentence, Mode::Foundation, Action::Separate, 1, 0, 0 },
        { 6, Category::Sentence, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 7, Category::Sentence, Mode::Context, Action::Order, 2, 1, 0 },
        { 8, Category::Sentence, Mode::Context, Action::Order, 2, 1, 0 },
        { 9, Category::Sentence, Mode::Context, Action::Order, 2, 1, 0 },
        { 10, Category::Sentence, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 11, Category::Sentence, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 12, Category::Sentence, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 13, Category::Sentence, Mode::Context, Action::Preserve, 2, 1, 0 },
        { 14, Category::Capital, Mode::Foundation, Action::Capitalize, 1, 0, 0 },
        { 15, Category::Capital, Mode::Foundation, Action::Capitalize, 1, 0, 0 },
        { 16, Category::Capital, Mode::Foundation, Action::Capitalize, 1, 0, 0 },
        { 17, Category::Capital, Mode::Foundation, Action::Capitalize, 1, 0, 0 },
        { 18, Category::Capital, Mode::Foundation, Action::Capitalize, 1, 0, 0 },
        { 19, Category::Capital, Mode::Context, Action::Capitalize, 2, 1, 0 },
        { 20, Category::Capital, Mode::Context, Action::Capitalize, 2, 1, 0 },
        { 21, Category::Capital, Mode::Context, Action::Capitalize, 2, 1, 0 },
        { 22, Category::Capital, Mode::Context, Action::Capitalize, 2, 1, 0 },
        { 23, Category::Capital, Mode::Style, Action::Capitalize, 3, 2, 0 },
        { 24, Category::Capital, Mode::Style, Action::Capitalize, 3, 2, 0 },
        { 25, Category::Capital, Mode::Context, Action::Preserve, 2, 1, 0 },
        { 26, Category::Capital, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 27, Category::Capital, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 28, Category::Capital, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 29, Category::Capital, Mode::Context, Action::Lowercase, 2, 1, 0 },
        { 30, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 31, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 32, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 33, Category::Comma, Mode::Context, Action::Omit, 2, 1, 0 },
        { 34, Category::Comma, Mode::Context, Action::Omit, 2, 1, 0 },
        { 35, Category::Comma, Mode::Context, Action::Omit, 2, 1, 0 },
        { 36, Category::Comma, Mode::Context, Action::Omit, 2, 1, 0 },
        { 37, Category::Comma, Mode::Foundation, Action::Separate, 1, 0, 0 },
        { 38, Category::Comma, Mode::Style, Action::Select, 3, 2, 0 },
        { 39, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 40, Category::Comma, Mode::Context, Action::Omit, 2, 1, 0 },
        { 41, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 42, Category::Comma, Mode::Context, Action::Omit, 2, 1, 0 },
        { 43, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 44, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 45, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 46, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 47, Category::Comma, Mode::Context, Action::Omit, 2, 1, 0 },
        { 48, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 49, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 50, Category::Comma, Mode::Context, Action::Insert, 2, 1, 0 },
        { 51, Category::Comma, Mode::Context, Action::Resolve, 2, 1, 0 },
        { 52, Category::Punctuation, Mode::Foundation, Action::Punctuate, 1, 0, 0 },
        { 53, Category::Punctuation, Mode::Foundation, Action::Punctuate, 1, 0, 0 },
        { 54, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 55, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 56, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 57, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 58, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 59, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 60, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 61, Category::Punctuation, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 62, Category::Punctuation, Mode::Style, Action::Punctuate, 3, 2, 0 },
        { 63, Category::Punctuation, Mode::Style, Action::Punctuate, 3, 2, 0 },
        { 64, Category::Punctuation, Mode::Context, Action::Omit, 2, 1, 0 },
        { 65, Category::Punctuation, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 66, Category::Quote, Mode::Foundation, Action::Punctuate, 1, 0, 0 },
        { 67, Category::Quote, Mode::Context, Action::Capitalize, 2, 1, 0 },
        { 68, Category::Quote, Mode::Context, Action::Lowercase, 2, 1, 0 },
        { 69, Category::Quote, Mode::Context, Action::Lowercase, 2, 1, 0 },
        { 70, Category::Quote, Mode::Style, Action::Punctuate, 3, 2, 0 },
        { 71, Category::Quote, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 72, Category::Quote, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 73, Category::Quote, Mode::Style, Action::Punctuate, 3, 2, 0 },
        { 74, Category::Quote, Mode::Context, Action::Preserve, 2, 1, 0 },
        { 75, Category::Quote, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 76, Category::Quote, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 77, Category::Apostrophe, Mode::Foundation, Action::Punctuate, 1, 0, 0 },
        { 78, Category::Apostrophe, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 79, Category::Apostrophe, Mode::Foundation, Action::Punctuate, 1, 0, 0 },
        { 80, Category::Apostrophe, Mode::Foundation, Action::Punctuate, 1, 0, 0 },
        { 81, Category::Apostrophe, Mode::Foundation, Action::Omit, 1, 0, 0 },
        { 82, Category::Apostrophe, Mode::Foundation, Action::Omit, 1, 0, 0 },
        { 83, Category::Apostrophe, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 84, Category::Apostrophe, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 85, Category::Apostrophe, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 86, Category::Apostrophe, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 87, Category::Apostrophe, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 88, Category::Spacing, Mode::Foundation, Action::Space, 1, 0, 0 },
        { 89, Category::Spacing, Mode::Foundation, Action::Space, 1, 0, 0 },
        { 90, Category::Spacing, Mode::Foundation, Action::Omit, 1, 0, 0 },
        { 91, Category::Spacing, Mode::Context, Action::Omit, 2, 1, 0 },
        { 92, Category::Spacing, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 93, Category::Spacing, Mode::Context, Action::Omit, 2, 1, 0 },
        { 94, Category::Spacing, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 95, Category::Spacing, Mode::Context, Action::Punctuate, 2, 1, 0 },
        { 96, Category::Spacing, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 97, Category::Spacing, Mode::Context, Action::Preserve, 2, 1, 0 },
        { 98, Category::Spacing, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 99, Category::Noun, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 100, Category::Noun, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 101, Category::Noun, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 102, Category::Noun, Mode::Context, Action::Select, 2, 1, 0 },
        { 103, Category::Noun, Mode::Context, Action::Select, 2, 1, 0 },
        { 104, Category::Noun, Mode::Foundation, Action::Agree, 1, 0, 0 },
        { 105, Category::Noun, Mode::Context, Action::Select, 2, 1, 0 },
        { 106, Category::Noun, Mode::Context, Action::Select, 2, 1, 0 },
        { 107, Category::Noun, Mode::Context, Action::Select, 2, 1, 0 },
        { 108, Category::Noun, Mode::Context, Action::Select, 2, 1, 0 },
        { 109, Category::Noun, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 110, Category::Verb, Mode::Foundation, Action::Agree, 1, 0, 0 },
        { 111, Category::Verb, Mode::Foundation, Action::Agree, 1, 0, 0 },
        { 112, Category::Verb, Mode::Foundation, Action::Agree, 1, 0, 0 },
        { 113, Category::Verb, Mode::Context, Action::Agree, 2, 1, 0 },
        { 114, Category::Verb, Mode::Context, Action::Agree, 2, 1, 0 },
        { 115, Category::Verb, Mode::Context, Action::Agree, 2, 1, 0 },
        { 116, Category::Verb, Mode::Context, Action::Agree, 2, 1, 0 },
        { 117, Category::Verb, Mode::Context, Action::Agree, 2, 1, 0 },
        { 118, Category::Verb, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 119, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 120, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 121, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 122, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 123, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 124, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 125, Category::Verb, Mode::Context, Action::Order, 2, 1, 0 },
        { 126, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 127, Category::Verb, Mode::Context, Action::Select, 2, 1, 0 },
        { 128, Category::Verb, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 129, Category::Pronoun, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 130, Category::Pronoun, Mode::Context, Action::Select, 2, 1, 0 },
        { 131, Category::Pronoun, Mode::Context, Action::Resolve, 2, 1, 0 },
        { 132, Category::Pronoun, Mode::Context, Action::Agree, 2, 1, 0 },
        { 133, Category::Pronoun, Mode::Context, Action::Select, 2, 1, 0 },
        { 134, Category::Pronoun, Mode::Foundation, Action::Select, 1, 0, 0 },
        { 135, Category::Pronoun, Mode::Context, Action::Select, 2, 1, 0 },
        { 136, Category::Pronoun, Mode::Context, Action::Select, 2, 1, 0 },
        { 137, Category::Pronoun, Mode::Context, Action::Select, 2, 1, 0 },
        { 138, Category::Pronoun, Mode::Context, Action::Select, 2, 1, 0 },
        { 139, Category::Pronoun, Mode::Context, Action::Select, 2, 1, 0 },
        { 140, Category::Syntax, Mode::Context, Action::Order, 2, 1, 0 },
        { 141, Category::Syntax, Mode::Context, Action::Order, 2, 1, 0 },
        { 142, Category::Syntax, Mode::Context, Action::Resolve, 2, 1, 0 },
        { 143, Category::Syntax, Mode::Context, Action::Resolve, 2, 1, 0 },
        { 144, Category::Syntax, Mode::Context, Action::Resolve, 2, 1, 0 },
        { 145, Category::Syntax, Mode::Context, Action::Order, 2, 1, 0 },
        { 146, Category::Syntax, Mode::Context, Action::Select, 2, 1, 0 },
        { 147, Category::Syntax, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 148, Category::Syntax, Mode::Context, Action::Select, 2, 1, 0 },
        { 149, Category::Syntax, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 150, Category::Syntax, Mode::Context, Action::Order, 2, 1, 0 },
        { 151, Category::Syntax, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 152, Category::Syntax, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 153, Category::Syntax, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 154, Category::Syntax, Mode::Context, Action::Recognize, 2, 1, 0 },
        { 155, Category::Syntax, Mode::Context, Action::Preserve, 2, 1, 0 },
        { 156, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 157, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 158, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 159, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 160, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 161, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 162, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 163, Category::Style, Mode::Style, Action::Select, 3, 2, 0 },
        { 164, Category::Style, Mode::Context, Action::Preserve, 2, 1, 0 },
        { 165, Category::Style, Mode::Context, Action::Resolve, 2, 1, 0 },
        { 166, Category::Style, Mode::Context, Action::Recognize, 2, 1, 0 },
        } };
        static_assert(RULES.size() == RULE_COUNT, "Rule count mismatch");

        bool MakeFolder(const std::wstring& folder)
        {
#ifdef _WIN32
            if (CreateDirectoryW(folder.c_str(), nullptr)) return true;
            if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
            DWORD attributes = GetFileAttributesW(folder.c_str());
            return attributes != INVALID_FILE_ATTRIBUTES &&
                (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
            const std::string path(folder.begin(), folder.end());
            if (::mkdir(path.c_str(), 0775) == 0) return true;
            struct stat info {};
            return ::stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
#endif
        }

        FILE* Open(const std::wstring& name, const wchar_t* mode)
        {
#ifdef _WIN32
            FILE* file = nullptr;
            if (_wfopen_s(&file, name.c_str(), mode) != 0) return nullptr;
            return file;
#else
            const std::string path(name.begin(), name.end());
            const std::string openMode(mode, mode + std::wcslen(mode));
            return std::fopen(path.c_str(), openMode.c_str());
#endif
        }

        void Remove(const std::wstring& name)
        {
#ifdef _WIN32
            DeleteFileW(name.c_str());
#else
            const std::string path(name.begin(), name.end());
            std::remove(path.c_str());
#endif
        }

        bool Replace(const std::wstring& from, const std::wstring& to)
        {
#ifdef _WIN32
            return MoveFileExW(from.c_str(), to.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
            const std::string oldPath(from.begin(), from.end());
            const std::string newPath(to.begin(), to.end());
            return std::rename(oldPath.c_str(), newPath.c_str()) == 0;
#endif
        }

        bool SameContents(const std::wstring& fileName, const std::vector<std::uint8_t>& bytes)
        {
            FILE* file = Open(fileName, L"rb");
            if (!file) return false;
            if (std::fseek(file, 0, SEEK_END) != 0)
            {
                std::fclose(file);
                return false;
            }
            const long size = std::ftell(file);
            if (size != static_cast<long>(bytes.size()) ||
                std::fseek(file, 0, SEEK_SET) != 0)
            {
                std::fclose(file);
                return false;
            }
            std::vector<std::uint8_t> existing(bytes.size());
            bool equal = std::fread(existing.data(), 1, existing.size(), file) == existing.size();
            equal = std::fclose(file) == 0 && equal;
            return equal && existing == bytes;
        }

        bool Save(const std::wstring& fileName, const std::vector<std::uint8_t>& bytes)
        {
            if (SameContents(fileName, bytes)) return true;
            const std::wstring temp = fileName + L".tmp";
            FILE* file = Open(temp, L"wb");
            if (!file) return false;
            const bool written = std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
            const bool closed = std::fclose(file) == 0;
            if (written && closed && Replace(temp, fileName)) return true;
            Remove(temp);
            return false;
        }

        void U16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
        {
            bytes.push_back(static_cast<std::uint8_t>(value));
            bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        }
        void U32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
        {
            U16(bytes, static_cast<std::uint16_t>(value));
            U16(bytes, static_cast<std::uint16_t>(value >> 16));
        }

        std::vector<std::uint8_t> Serialize()
        {
            std::vector<std::uint8_t> payload;
            payload.reserve(RULE_COUNT * 8);
            for (const RuleNode& rule : RULES)
            {
                U16(payload, rule.id);
                payload.push_back(static_cast<std::uint8_t>(rule.category));
                payload.push_back(static_cast<std::uint8_t>(rule.mode));
                payload.push_back(static_cast<std::uint8_t>(rule.action));
                payload.push_back(rule.priority);
                payload.push_back(rule.flags);
                payload.push_back(rule.reserved);
            }

            // Small deterministic versioned cache: header 16 + exactly 8 bytes per rule.
            std::uint32_t hash = 2166136261u; // FNV-1a over the payload
            for (std::uint8_t b : payload)
            {
                hash ^= b;
                hash *= 16777619u;
            }
            std::vector<std::uint8_t> bytes;
            bytes.reserve(16 + payload.size());
            const char signature[8] = { 'S','A','O','E','N','G','R','1' };
            bytes.insert(bytes.end(), signature, signature + 8);
            U16(bytes, CACHE_VERSION);
            U16(bytes, static_cast<std::uint16_t>(RULE_COUNT));
            U32(bytes, hash);
            bytes.insert(bytes.end(), payload.begin(), payload.end());
            return bytes;
        }

        void MakeGroups(OutputNode& output)
        {
            for (std::size_t i = 0; i < RULES.size(); ++i)
            {
                const Category category = RULES[i].category;
                const std::size_t index = static_cast<std::size_t>(category) - 1;
                GroupNode& group = output.groups[index];
                group.category = category;
                if (!group.firstId) group.firstId = RULES[i].id;
                ++group.count;
            }
        }
    }

    const OutputNode& BuildAt(const std::wstring& folder)
    {
        if (Cache.ready) return Cache;
        Cache = OutputNode{};
        if (folder.empty() || !MakeFolder(folder))
        {
            Cache.error = L"Cannot create SAO English Rules cache folder.";
            return Cache;
        }
        const wchar_t separator =
            (folder.back() == L'\\' || folder.back() == L'/') ? L'\0' :
#ifdef _WIN32
            L'\\';
#else
            L'/';
#endif
        Cache.rules.name = L"SAO_English_Rules.cache";
        Cache.rules.path = folder;
        if (separator) Cache.rules.path += separator;
        Cache.rules.path += Cache.rules.name;

        const auto bytes = Serialize();
        if (!Save(Cache.rules.path, bytes))
        {
            Cache.error = L"Cannot write or verify compact English rules cache.";
            return Cache;
        }
        Cache.rules.bytes = static_cast<std::uint32_t>(bytes.size());
        Cache.rules.ruleCount = static_cast<std::uint16_t>(RULE_COUNT);
        Cache.rules.ready = true;
        MakeGroups(Cache);
        Cache.ready = true;
        return Cache;
    }

    const OutputNode& Build()
    {
        if (Cache.ready) return Cache;
#ifdef _WIN32
        wchar_t executable[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (!length || length >= MAX_PATH)
        {
            Cache.error = L"Cannot locate executable for English Rules cache.";
            return Cache;
        }
        std::wstring folder(executable, length);
        const std::size_t end = folder.find_last_of(L"\\/");
        if (end == std::wstring::npos)
        {
            Cache.error = L"Executable folder cannot be determined.";
            return Cache;
        }
        folder.resize(end + 1);
        folder += L"Cache";
        if (!MakeFolder(folder))
        {
            Cache.error = L"Cannot create main cache folder.";
            return Cache;
        }
        return BuildAt(folder + L"\\SAO_English_Rules_Math");
#else
        if (!MakeFolder(L"Cache"))
        {
            Cache.error = L"Cannot create main cache folder.";
            return Cache;
        }
        return BuildAt(L"Cache/SAO_English_Rules_Math");
#endif
    }

    const RuleNode* Find(std::uint16_t id)
    {
        if (!Cache.ready || id == 0 || id > RULE_COUNT) return nullptr;
        const RuleNode& rule = RULES[id - 1];
        return rule.id == id ? &rule : nullptr;
    }

    GroupNode Group(Category category)
    {
        const std::size_t index = static_cast<std::size_t>(category);
        if (!Cache.ready || index == 0 || index > CATEGORY_COUNT) return {};
        return Cache.groups[index - 1];
    }
}