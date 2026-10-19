#include "SAO_Definitions_Math.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace SAO_Definitions_Math
{
    OutputNode Cache{};
    namespace
    {
        constexpr std::array<char, 8> MAGIC{ 'S','A','O','D','E','F','0','1' };
        constexpr std::uint32_t VERSION = 1;
        constexpr std::size_t HEADER_BYTES = 44;
        constexpr std::uint64_t FNV_BASE = 14695981039346656037ULL;
        constexpr std::uint64_t FNV_PRIME = 1099511628211ULL;
        constexpr std::uint32_t MAX_DATA_BYTES = 32u * 1024u * 1024u;

        struct Word
        {
            std::uint32_t id = 0;
            std::string spelling;
            std::uint32_t firstSense = 0;
            std::uint32_t senseCount = 0;
        };
        struct Sense
        {
            std::uint32_t id = 0;
            std::uint32_t wordId = 0;
            std::uint32_t pos = 0;
            std::string gloss;
        };
        struct Index
        {
            std::uint32_t id = 0;
            std::string word;
            std::uint32_t firstSense = 0;
            std::uint32_t senseCount = 0;
        };
        struct SenseIndex
        {
            std::uint32_t id = 0;
            std::uint32_t pos = 0;
            std::uint32_t offset = 0;
            std::uint32_t length = 0;
        };

        std::vector<Index> g_words;
        // Word IDs keep their original order; lookup order is independent.
        std::vector<std::uint32_t> g_lookup;
        std::vector<SenseIndex> g_senses;
        std::uint64_t g_glossBase = 0;

        std::uint64_t Hash(std::string_view text, std::uint64_t seed = FNV_BASE)
        {
            for (unsigned char b : text)
            {
                seed ^= b;
                seed *= FNV_PRIME;
            }
            return seed;
        }
        void Add32(std::string& bytes, std::uint32_t value)
        {
            for (unsigned i = 0; i < 4; ++i)
                bytes.push_back(static_cast<char>((value >> (8 * i)) & 255u));
        }
        void Add64(std::string& bytes, std::uint64_t value)
        {
            for (unsigned i = 0; i < 8; ++i)
                bytes.push_back(static_cast<char>((value >> (8 * i)) & 255u));
        }
        bool Read32(const std::string& bytes, std::size_t& at, std::uint32_t& out)
        {
            if (at > bytes.size() || bytes.size() - at < 4) return false;
            out = 0;
            for (unsigned i = 0; i < 4; ++i)
                out |= std::uint32_t(static_cast<unsigned char>(bytes[at++])) << (8 * i);
            return true;
        }
        bool Read64(const std::string& bytes, std::size_t& at, std::uint64_t& out)
        {
            if (at > bytes.size() || bytes.size() - at < 8) return false;
            out = 0;
            for (unsigned i = 0; i < 8; ++i)
                out |= std::uint64_t(static_cast<unsigned char>(bytes[at++])) << (8 * i);
            return true;
        }
        bool LoadText(const std::filesystem::path& path, std::string& text)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) return false;
            file.seekg(0, std::ios::end);
            const auto bytes = file.tellg();
            if (bytes < 0 || bytes > MAX_DATA_BYTES) return false;
            file.seekg(0);
            text.assign(std::istreambuf_iterator<char>(file), {});
            return file.good() || file.eof();
        }
        bool ParseUint(const std::string& text, std::uint32_t& value)
        {
            if (text.empty()) return false;
            std::uint64_t out = 0;
            for (unsigned char c : text)
            {
                if (c < '0' || c > '9') return false;
                if (out > (UINT32_MAX - (c - '0')) / 10) return false;
                out = out * 10 + (c - '0');
            }
            value = static_cast<std::uint32_t>(out);
            return true;
        }
        std::uint32_t PosCode(std::string_view pos)
        {
            constexpr std::array<std::string_view, 10> names{
                "noun","verb","adjective","adverb","pronoun","preposition",
                "conjunction","determiner","article","interjection"
            };
            for (std::uint32_t i = 0; i < names.size(); ++i)
                if (pos == names[i]) return i + 1;
            return 0;
        }
        std::string PosName(std::uint32_t pos)
        {
            constexpr std::array<std::string_view, 10> names{
                "noun","verb","adjective","adverb","pronoun","preposition",
                "conjunction","determiner","article","interjection"
            };
            return pos >= 1 && pos <= names.size() ?
                std::string(names[pos - 1]) : std::string();
        }
        bool ValidWord(std::string_view word)
        {
            if (word.empty()) return false;
            for (unsigned char c : word) if (c < 'a' || c > 'z') return false;
            return true;
        }
        std::vector<std::string> Cells(const std::string& line, std::size_t expected)
        {
            std::vector<std::string> fields;
            std::size_t start = 0;
            for (std::size_t i = 0; i < expected - 1; ++i)
            {
                auto found = line.find('\t', start);
                if (found == std::string::npos) return {};
                fields.push_back(line.substr(start, found - start));
                start = found + 1;
            }
            fields.push_back(line.substr(start));
            return fields;
        }
        bool Parse(const std::string& wordText, const std::string& senseText,
            std::vector<Word>& words, std::vector<Sense>& senses)
        {
            std::size_t begin = 0;
            while (begin < wordText.size())
            {
                auto end = wordText.find('\n', begin);
                if (end == std::string::npos) end = wordText.size();
                std::string line = wordText.substr(begin, end - begin);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                begin = end + 1;
                if (line.empty() || line[0] == '#') continue;
                auto fields = Cells(line, 2);
                std::uint32_t id = 0;
                if (fields.size() != 2 || !ParseUint(fields[0], id) ||
                    id != words.size() + 1 || !ValidWord(fields[1]) ||
                    fields[1].size() > 64) return false;
                words.push_back({ id, fields[1], 0, 0 });
            }
            if (words.empty()) return false;
            // Old word IDs remain stable when new vocabulary is appended.
            // Reject duplicates independently of the numeric ordering.
            std::vector<std::string_view> spellings;
            spellings.reserve(words.size());
            for (const auto& word : words) spellings.push_back(word.spelling);
            std::sort(spellings.begin(), spellings.end());
            if (std::adjacent_find(spellings.begin(), spellings.end()) != spellings.end()) return false;
            begin = 0;
            while (begin < senseText.size())
            {
                auto end = senseText.find('\n', begin);
                if (end == std::string::npos) end = senseText.size();
                std::string line = senseText.substr(begin, end - begin);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                begin = end + 1;
                if (line.empty() || line[0] == '#') continue;
                auto fields = Cells(line, 4);
                std::uint32_t id = 0, wordId = 0;
                if (fields.size() != 4 || !ParseUint(fields[0], id) ||
                    !ParseUint(fields[1], wordId) || id != senses.size() + 1 ||
                    !wordId || wordId > words.size() || fields[3].empty()) return false;
                const auto pos = PosCode(fields[2]);
                if (!pos) return false;
                if (!senses.empty() && wordId < senses.back().wordId) return false;
                Word& word = words[wordId - 1];
                if (!word.senseCount) word.firstSense = static_cast<std::uint32_t>(senses.size());
                ++word.senseCount;
                senses.push_back({ id, wordId, pos, fields[3] });
            }
            if (senses.empty()) return false;
            for (const auto& word : words) if (!word.senseCount) return false;
            return true;
        }
        std::string Encode(const std::vector<Word>& words,
            const std::vector<Sense>& senses, std::uint64_t sourceHash)
        {
            std::string wordBytes, glossBytes, wordTable, senseTable;
            for (const auto& word : words)
            {
                Add32(wordTable, word.id);
                Add32(wordTable, word.firstSense);
                Add32(wordTable, word.senseCount);
                Add32(wordTable, static_cast<std::uint32_t>(wordBytes.size()));
                Add32(wordTable, static_cast<std::uint32_t>(word.spelling.size()));
                wordBytes += word.spelling;
            }
            for (const auto& sense : senses)
            {
                Add32(senseTable, sense.id);
                Add32(senseTable, sense.pos);
                Add32(senseTable, static_cast<std::uint32_t>(glossBytes.size()));
                Add32(senseTable, static_cast<std::uint32_t>(sense.gloss.size()));
                glossBytes += sense.gloss;
            }
            std::string payload = wordTable + senseTable + wordBytes + glossBytes;
            std::string output(MAGIC.data(), MAGIC.size());
            Add32(output, VERSION);
            Add32(output, static_cast<std::uint32_t>(words.size()));
            Add32(output, static_cast<std::uint32_t>(senses.size()));
            Add32(output, static_cast<std::uint32_t>(wordBytes.size()));
            Add32(output, static_cast<std::uint32_t>(glossBytes.size()));
            Add64(output, sourceHash);
            Add64(output, Hash(payload));
            return output + payload;
        }
        bool DecodeIndex(const std::string& binary, std::uint64_t expectedSource,
            std::vector<Index>& words, std::vector<SenseIndex>& senses,
            std::uint64_t& glossBase)
        {
            if (binary.size() < HEADER_BYTES ||
                !std::equal(MAGIC.begin(), MAGIC.end(), binary.begin())) return false;
            std::size_t at = MAGIC.size();
            std::uint32_t version = 0, wc = 0, sc = 0, wordBytes = 0, glossBytes = 0;
            std::uint64_t sourceHash = 0, checksum = 0;
            if (!Read32(binary, at, version) || !Read32(binary, at, wc) ||
                !Read32(binary, at, sc) || !Read32(binary, at, wordBytes) ||
                !Read32(binary, at, glossBytes) || !Read64(binary, at, sourceHash) ||
                !Read64(binary, at, checksum) || version != VERSION ||
                sourceHash != expectedSource || wc == 0 || sc == 0 ||
                wc > 100000 || sc > 1000000) return false;
            const std::uint64_t expected = HEADER_BYTES + std::uint64_t(wc) * 20 +
                std::uint64_t(sc) * 16 + wordBytes + glossBytes;
            if (expected != binary.size() || Hash(std::string_view(binary).substr(HEADER_BYTES)) != checksum) return false;

            const std::size_t baseWords = HEADER_BYTES + std::size_t(wc) * 20 + std::size_t(sc) * 16;
            const std::size_t baseGloss = baseWords + wordBytes;
            for (std::uint32_t i = 0; i < wc; ++i)
            {
                std::uint32_t id = 0, first = 0, count = 0, offset = 0, length = 0;
                if (!Read32(binary, at, id) || !Read32(binary, at, first) ||
                    !Read32(binary, at, count) || !Read32(binary, at, offset) ||
                    !Read32(binary, at, length) || id != i + 1 || !count ||
                    std::uint64_t(first) + count > sc || std::uint64_t(offset) + length > wordBytes) return false;
                std::string word(binary.data() + baseWords + offset, length);
                if (!ValidWord(word)) return false;
                words.push_back({ id,std::move(word),first,count });
            }
            for (std::uint32_t i = 0; i < sc; ++i)
            {
                std::uint32_t id = 0, pos = 0, offset = 0, length = 0;
                if (!Read32(binary, at, id) || !Read32(binary, at, pos) ||
                    !Read32(binary, at, offset) || !Read32(binary, at, length) ||
                    id != i + 1 || pos < 1 || pos>10 || !length ||
                    std::uint64_t(offset) + length > glossBytes) return false;
                senses.push_back({ id,pos,offset,length });
            }
            glossBase = baseGloss;
            return true;
        }
        bool Save(const std::filesystem::path& target, const std::string& binary)
        {
            const auto temporary = std::filesystem::path(target.wstring() + L".tmp");
            {
                std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                if (!out || !out.write(binary.data(), static_cast<std::streamsize>(binary.size()))) return false;
                out.close();
                if (!out) return false;
            }
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), target.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return false;
#else
            std::error_code ec;
            std::filesystem::rename(temporary, target, ec);
            if (ec) return false;
#endif
            return true;
        }
        bool SortLookup()
        {
            g_lookup.clear();
            g_lookup.reserve(g_words.size());
            for (std::uint32_t i = 0; i < g_words.size(); ++i) g_lookup.push_back(i);
            std::sort(g_lookup.begin(), g_lookup.end(), [](std::uint32_t a, std::uint32_t b) {
                return g_words[a].word < g_words[b].word;
                });
            for (std::size_t i = 1; i < g_lookup.size(); ++i)
                if (g_words[g_lookup[i - 1]].word == g_words[g_lookup[i]].word) return false;
            return true;
        }

        std::filesystem::path ExeFolder()
        {
#ifdef _WIN32
            wchar_t path[32768]{};
            const DWORD n = GetModuleFileNameW(nullptr, path, 32768);
            if (!n || n >= 32768) return {};
            return std::filesystem::path(path).parent_path();
#else
            char path[4096]{};
            const auto n = readlink("/proc/self/exe", path, sizeof(path) - 1);
            if (n <= 0) return {};
            path[n] = 0;
            return std::filesystem::path(path).parent_path();
#endif
        }
    }

    const OutputNode& Build()
    {
        const auto exe = ExeFolder();
        if (exe.empty())
        {
            Cache = OutputNode{};
            Cache.error = L"Cannot locate executable directory.";
            return Cache;
        }
        // Locate the project asset folder from the executable's own ancestors.
        // This PC / Desktop is a shell location; no OneDrive or user-path assumptions.
        auto folder = exe;
        for (std::size_t depth = 0; depth < 12; ++depth)
        {
            const auto definitions = folder / "SAO_I_E Assets" / "Alphabet" / "Definitions";
            std::error_code ec;
            if (std::filesystem::is_directory(definitions, ec))
                return Build(definitions / "SAO_Definitions_Words.tsv",
                    definitions / "SAO_Definitions_Senses.tsv",
                    exe / "Cache" / "SAO_Definitions_Math");
            if (folder == folder.root_path() || folder == folder.parent_path()) break;
            folder = folder.parent_path();
        }
        Cache = OutputNode{};
        Cache.error = L"Cannot locate SAO_I_E Assets\\Alphabet\\Definitions beside the project.";
        return Cache;
    }

    const OutputNode& Build(const std::filesystem::path& wordsFile,
        const std::filesystem::path& sensesFile,
        const std::filesystem::path& cacheFolder)
    {
        Cache = OutputNode{};
        g_words.clear();
        g_lookup.clear();
        g_senses.clear();
        g_glossBase = 0;
        std::string wordText, senseText;
        if (!LoadText(wordsFile, wordText) || !LoadText(sensesFile, senseText))
        {
            Cache.error = L"Cannot read the Words and Senses source TSV files.";
            return Cache;
        }
        const std::uint64_t sourceHash = Hash(senseText, Hash(std::string_view("\0", 1), Hash(wordText)));
        const auto path = cacheFolder / "SAO_Definitions.cache";
        std::error_code ec;
        std::filesystem::create_directories(cacheFolder, ec);
        if (ec)
        {
            Cache.error = L"Cannot create definitions cache folder.";
            return Cache;
        }
        std::string binary;
        bool reuse = LoadText(path, binary) &&
            DecodeIndex(binary, sourceHash, g_words, g_senses, g_glossBase);
        if (!reuse)
        {
            g_words.clear(); g_senses.clear(); g_glossBase = 0;
            std::vector<Word> words;
            std::vector<Sense> senses;
            if (!Parse(wordText, senseText, words, senses))
            {
                Cache.error = L"Invalid words/senses source data or mismatched word IDs.";
                return Cache;
            }
            binary = Encode(words, senses, sourceHash);
            if (!Save(path, binary) ||
                !DecodeIndex(binary, sourceHash, g_words, g_senses, g_glossBase))
            {
                Cache.error = L"Cannot create or validate definitions bitmap-independent cache.";
                return Cache;
            }
        }
        if (!SortLookup())
        {
            Cache.error = L"Definitions words contain duplicate spellings.";
            g_words.clear(); g_senses.clear(); g_lookup.clear();
            return Cache;
        }
        Cache.cache.name = L"SAO_Definitions.cache";
        Cache.cache.path = path.wstring();
        Cache.cache.wordCount = static_cast<std::uint32_t>(g_words.size());
        Cache.cache.definitionCount = static_cast<std::uint32_t>(g_senses.size());
        Cache.cache.contentHash = sourceHash;
        Cache.cache.ready = true;
        Cache.ready = true;
        return Cache;
    }

    WordResult Find(std::string_view input)
    {
        WordResult result{};
        if (!Cache.ready || input.empty()) return result;
        std::string term(input);
        for (char& c : term) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (!ValidWord(term)) return result;
        auto it = std::lower_bound(g_lookup.begin(), g_lookup.end(), term,
            [](std::uint32_t index, const std::string& text) { return g_words[index].word < text; });
        if (it == g_lookup.end() || g_words[*it].word != term) return result;
        const Index& word = g_words[*it];
        std::ifstream file(std::filesystem::path(Cache.cache.path), std::ios::binary);
        if (!file) return result;
        result.id = word.id;
        result.word = word.word;
        for (std::uint32_t i = 0; i < word.senseCount; ++i)
        {
            const SenseIndex& sense = g_senses[word.firstSense + i];
            file.seekg(static_cast<std::streamoff>(g_glossBase + sense.offset));
            if (!file) return WordResult{};
            std::string gloss(sense.length, '\0');
            if (!file.read(gloss.data(), sense.length)) return WordResult{};
            result.meanings.push_back({ sense.id,PosName(sense.pos),std::move(gloss) });
        }
        result.found = true;
        return result;
    }
}