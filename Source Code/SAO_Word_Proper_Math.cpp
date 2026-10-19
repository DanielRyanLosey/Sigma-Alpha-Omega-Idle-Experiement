#include "SAO_Word_Proper_Math.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace SAO_Word_Proper_Math
{
    OutputNode Cache{};
    namespace
    {
        constexpr char MAGIC[] = "SAOWP001";
        constexpr std::uint32_t VERSION = 1;
        constexpr std::uint64_t OFFSET = 14695981039346656037ULL;
        constexpr std::uint64_t PRIME = 1099511628211ULL;
        constexpr std::size_t HEADER_BYTES = 40;
        constexpr std::uintmax_t MAX_TEXT_BYTES = 4 * 1024 * 1024;
        struct WordIndex
        {
            std::uint32_t id = 0, first = 0, count = 0;
            std::string spelling;
        };
        std::vector<WordIndex> g_words;
        std::vector<std::uint32_t> g_sortedWordIds; // Search only; source IDs stay unchanged.
        std::vector<Usage> g_uses;
        std::vector<std::string> g_third_person_forms; // One build-time index, reused per check.
        constexpr std::array<std::string_view, 12> POS_NAMES{
            "noun", "verb", "adjective", "adverb", "pronoun", "preposition",
            "conjunction", "determiner", "article", "interjection", "auxiliary", "modal"
        };
        constexpr std::array<std::string_view, 27> FRAME_NAMES{
            "noun_phrase", "verb_unspecified", "verb_intransitive", "verb_transitive",
            "verb_ditransitive", "verb_flexible", "verb_to", "verb_object_to",
            "adjective_both", "adverb_modifier", "pronoun_subject", "pronoun_object",
            "pronoun_both", "pronoun_possessive", "preposition_object", "to_or_preposition",
            "conjunction_coord", "conjunction_sub", "conjunction_context",
            "determiner_before_noun", "article_before_noun", "interjection",
            "auxiliary_be", "auxiliary_have", "auxiliary_do", "modal_bare_verb", "negation"
        };
        template <typename E, std::size_t N>
        E Code(std::string_view name, const std::array<std::string_view, N>& names)
        {
            for (std::size_t i = 0; i < names.size(); ++i)
                if (name == names[i]) return static_cast<E>(i + 1);
            return static_cast<E>(0);
        }
        std::uint64_t Hash(std::string_view text, std::uint64_t seed = OFFSET)
        {
            for (unsigned char byte : text) { seed ^= byte; seed *= PRIME; }
            return seed;
        }
        void W8(std::string& out, std::uint8_t n) { out.push_back(static_cast<char>(n)); }
        void W16(std::string& out, std::uint16_t n)
        {
            for (int i = 0; i < 2; ++i) W8(out, static_cast<std::uint8_t>(n >> (8 * i)));
        }
        void W32(std::string& out, std::uint32_t n)
        {
            for (int i = 0; i < 4; ++i) W8(out, static_cast<std::uint8_t>(n >> (8 * i)));
        }
        void W64(std::string& out, std::uint64_t n)
        {
            for (int i = 0; i < 8; ++i) W8(out, static_cast<std::uint8_t>(n >> (8 * i)));
        }
        bool RNum(const std::string& buf, std::size_t& at, std::size_t bytes, std::uint64_t& result)
        {
            if (at > buf.size() || bytes > buf.size() - at) return false;
            result = 0;
            for (std::size_t i = 0; i < bytes; ++i)
                result |= std::uint64_t(static_cast<unsigned char>(buf[at++])) << (8 * i);
            return true;
        }
        bool LoadText(const std::filesystem::path& path, std::string& text)
        {
            std::ifstream input(path, std::ios::binary | std::ios::ate);
            if (!input) return false;
            const auto length = input.tellg();
            if (length<0 || static_cast<std::uintmax_t>(length)>MAX_TEXT_BYTES) return false;
            text.resize(static_cast<std::size_t>(length));
            input.seekg(0);
            if (!text.empty() && !input.read(text.data(), length)) return false;
            return true;
        }
        bool Number(std::string_view source, std::uint32_t& value)
        {
            if (source.empty()) return false;
            std::uint32_t number = 0;
            for (char c : source)
            {
                if (c < '0' || c>'9' || number > (UINT32_MAX - std::uint32_t(c - '0')) / 10) return false;
                number = number * 10 + std::uint32_t(c - '0');
            }
            value = number; return true;
        }
        std::vector<std::string_view> Cells(std::string_view line, std::size_t expected)
        {
            std::vector<std::string_view> cells;
            std::size_t start = 0;
            for (std::size_t i = 1; i < expected; ++i)
            {
                const auto tab = line.find('\t', start);
                if (tab == std::string_view::npos) return {};
                cells.push_back(line.substr(start, tab - start)); start = tab + 1;
            }
            cells.push_back(line.substr(start));
            if (cells.back().find('\t') != std::string_view::npos) return {};
            return cells;
        }
        template<class F> bool Lines(std::string_view text, F fn)
        {
            std::size_t start = 0;
            while (start < text.size())
            {
                auto end = text.find('\n', start);
                if (end == std::string_view::npos) end = text.size();
                auto line = text.substr(start, end - start);
                if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
                start = end + 1;
                if (line.empty() || line[0] == '#') continue;
                if (!fn(line)) return false;
            }
            return true;
        }
        bool WordSpelling(std::string_view value)
        {
            if (value.empty() || value.size() > 64) return false;
            for (char c : value) if (c < 'a' || c>'z') return false;
            return true;
        }
        bool Parse(std::string_view names, std::string_view usages,
            std::vector<WordIndex>& words, std::vector<Usage>& uses)
        {
            if (!Lines(names, [&](std::string_view line)
                {
                    auto c = Cells(line, 2); std::uint32_t id = 0;
                    if (c.size() != 2 || !Number(c[0], id) || id != words.size() + 1 ||
                        id > UINT16_MAX || !WordSpelling(c[1])) return false;
                    words.push_back({ id,0,0,std::string(c[1]) }); return true;
                }) || words.empty()) return false;
            if (!Lines(usages, [&](std::string_view line)
                {
                    auto c = Cells(line, 6);
                    std::uint32_t sid = 0, wid = 0, features = 0, curated = 0;
                    if (c.size() != 6 || !Number(c[0], sid) || !Number(c[1], wid) ||
                        !Number(c[4], features) || !Number(c[5], curated) ||
                        sid != uses.size() + 1 || wid == 0 || wid > words.size() ||
                        features > UINT16_MAX || curated > 1 ||
                        (!uses.empty() && wid < uses.back().wordId)) return false;
                    const auto pos = Code<Part>(c[2], POS_NAMES);
                    const auto frame = Code<Frame>(c[3], FRAME_NAMES);
                    if (pos == Part::Unknown || frame == Frame::Unknown) return false;
                    auto& word = words[wid - 1];
                    if (word.count == 0) word.first = static_cast<std::uint32_t>(uses.size());
                    ++word.count;
                    uses.push_back({ sid,static_cast<std::uint16_t>(wid),pos,frame,
                                    static_cast<std::uint16_t>(features),curated != 0 });
                    return true;
                }) || uses.empty()) return false;
            for (const auto& w : words) if (!w.count) return false;
            return true;
        }
        std::string Encode(const std::vector<WordIndex>& words,
            const std::vector<Usage>& uses, std::uint64_t sourceHash)
        {
            std::string names, tables, entries;
            for (const auto& w : words)
            {
                W32(tables, w.id); W32(tables, w.first); W16(tables, static_cast<std::uint16_t>(w.count));
                W16(tables, static_cast<std::uint16_t>(w.spelling.size()));
                W32(tables, static_cast<std::uint32_t>(names.size()));
                names += w.spelling;
            }
            for (const auto& u : uses)
            {
                W32(entries, u.senseId); W16(entries, u.wordId);
                W8(entries, static_cast<std::uint8_t>(u.part));
                W8(entries, static_cast<std::uint8_t>(u.frame));
                W16(entries, u.features); W8(entries, u.curated ? 1 : 0); W8(entries, 0);
            }
            const std::string payload = tables + entries + names;
            std::string cache(MAGIC, 8);
            W32(cache, VERSION); W32(cache, static_cast<std::uint32_t>(words.size()));
            W32(cache, static_cast<std::uint32_t>(uses.size()));
            W32(cache, static_cast<std::uint32_t>(names.size()));
            W64(cache, sourceHash); W64(cache, Hash(payload));
            return cache + payload;
        }
        bool Decode(const std::string& encoded, std::uint64_t sourceHash,
            std::vector<WordIndex>& words, std::vector<Usage>& uses)
        {
            if (encoded.size() < HEADER_BYTES || !std::equal(MAGIC, MAGIC + 8, encoded.data())) return false;
            std::size_t at = 8; std::uint64_t version = 0, wc = 0, uc = 0, letters = 0, origin = 0, checksum = 0;
            if (!RNum(encoded, at, 4, version) || !RNum(encoded, at, 4, wc) ||
                !RNum(encoded, at, 4, uc) || !RNum(encoded, at, 4, letters) ||
                !RNum(encoded, at, 8, origin) || !RNum(encoded, at, 8, checksum) ||
                version != VERSION || origin != sourceHash || wc == 0 || wc > 65535 ||
                uc == 0 || uc > 100000 || HEADER_BYTES + wc * 16 + uc * 12 + letters != encoded.size() ||
                Hash(std::string_view(encoded).substr(HEADER_BYTES)) != checksum) return false;
            const std::size_t namesBase = HEADER_BYTES + wc * 16 + uc * 12;
            for (std::uint32_t i = 0; i < wc; ++i)
            {
                std::uint64_t id = 0, first = 0, count = 0, length = 0, offset = 0;
                if (!RNum(encoded, at, 4, id) || !RNum(encoded, at, 4, first) ||
                    !RNum(encoded, at, 2, count) || !RNum(encoded, at, 2, length) ||
                    !RNum(encoded, at, 4, offset) || id != i + 1 || !count ||
                    first + count > uc || offset + length > letters) return false;
                const std::string spelling(encoded.data() + namesBase + offset, static_cast<std::size_t>(length));
                if (!WordSpelling(spelling)) return false;
                words.push_back({ static_cast<std::uint32_t>(id),static_cast<std::uint32_t>(first),
                                 static_cast<std::uint32_t>(count),spelling });
            }
            std::size_t prevWid = 0;
            for (std::uint32_t i = 0; i < uc; ++i)
            {
                std::uint64_t sid = 0, wid = 0, pos = 0, frame = 0, flags = 0, curated = 0, reserved = 0;
                if (!RNum(encoded, at, 4, sid) || !RNum(encoded, at, 2, wid) ||
                    !RNum(encoded, at, 1, pos) || !RNum(encoded, at, 1, frame) ||
                    !RNum(encoded, at, 2, flags) || !RNum(encoded, at, 1, curated) ||
                    !RNum(encoded, at, 1, reserved) || sid != i + 1 || !wid || wid > wc ||
                    wid<prevWid || pos>POS_NAMES.size() || pos == 0 ||
                    frame > FRAME_NAMES.size() || frame == 0 || curated > 1 || reserved != 0)
                    return false;
                prevWid = static_cast<std::size_t>(wid);
                uses.push_back({ static_cast<std::uint32_t>(sid),static_cast<std::uint16_t>(wid),
                                static_cast<Part>(pos),static_cast<Frame>(frame),
                                static_cast<std::uint16_t>(flags),curated != 0 });
            }
            for (const auto& word : words)
                for (std::uint32_t i = word.first; i < word.first + word.count; ++i)
                    if (uses[i].wordId != word.id) return false;
            return true;
        }
        bool SortLookup()
        {
            g_sortedWordIds.clear();
            g_sortedWordIds.reserve(g_words.size());
            for (std::uint32_t id = 0; id < g_words.size(); ++id)
                g_sortedWordIds.push_back(id);
            std::sort(g_sortedWordIds.begin(), g_sortedWordIds.end(),
                [](std::uint32_t a, std::uint32_t b) {
                    return g_words[a].spelling < g_words[b].spelling;
                });
            for (std::size_t i = 1; i < g_sortedWordIds.size(); ++i)
                if (g_words[g_sortedWordIds[i - 1]].spelling == g_words[g_sortedWordIds[i]].spelling)
                    return false;
            return true;
        }
        std::string Lower(std::string_view text)
        {
            std::string out(text);
            for (char& c : out) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
            return out;
        }
        const WordIndex* Index(std::string_view input)
        {
            if (!Cache.ready) return nullptr;
            const std::string name = Lower(input);
            auto it = std::lower_bound(g_sortedWordIds.begin(), g_sortedWordIds.end(), name,
                [](std::uint32_t id, const std::string& spelling) {
                    return g_words[id].spelling < spelling;
                });
            if (it == g_sortedWordIds.end() || g_words[*it].spelling != name) return nullptr;
            return &g_words[*it];
        }
        bool Is(const WordIndex* word, Part part)
        {
            if (!word) return false;
            for (std::uint32_t i = word->first; i < word->first + word->count; ++i)
                if (g_uses[i].part == part) return true;
            return false;
        }
        bool HasFrame(const WordIndex* word, Frame frame)
        {
            if (!word) return false;
            for (std::uint32_t i = word->first; i < word->first + word->count; ++i)
                if (g_uses[i].frame == frame) return true;
            return false;
        }
        bool IsObject(const WordIndex* word)
        {
            return Is(word, Part::Noun) || HasFrame(word, Frame::PronounObject) ||
                HasFrame(word, Frame::PronounBoth) || Is(word, Part::Determiner) ||
                Is(word, Part::Article);
        }
        bool Save(const std::filesystem::path& path, const std::string& binary)
        {
            auto temporary = path;
            temporary += L".tmp";
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                if (!file || !file.write(binary.data(), static_cast<std::streamsize>(binary.size()))) return false;
                file.close(); if (!file) return false;
            }
            std::error_code ec;
#ifdef _WIN32
            if (!MoveFileExW(temporary.c_str(), path.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                return false;
#else
            std::filesystem::rename(temporary, path, ec);
            if (ec) return false;
#endif
            return true;
        }
        std::filesystem::path ExecutableFolder()
        {
#ifdef _WIN32
            wchar_t file[32768]{};
            const DWORD n = GetModuleFileNameW(nullptr, file, 32768);
            return n && n < 32768 ? std::filesystem::path(file).parent_path() : std::filesystem::path{};
#else
            char file[4096]{};
            auto n = readlink("/proc/self/exe", file, sizeof(file) - 1);
            if (n <= 0) return {};
            file[n] = 0; return std::filesystem::path(file).parent_path();
#endif
        }
        bool Ending(std::string_view text, std::string_view ending)
        {
            return text.size() >= ending.size() && text.substr(text.size() - ending.size()) == ending;
        }
        bool IsVowel(char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; }
        bool IsConsonant(char c) { return c >= 'a' && c <= 'z' && !IsVowel(c); }
        std::string PastOrThird(std::string_view text, bool third)
        {
            std::string word(text);
            if (Ending(word, "y") && word.size() > 1 && IsConsonant(word[word.size() - 2]))
                return word.substr(0, word.size() - 1) + (third ? "ies" : "ied");
            if (third && (Ending(word, "sh") || Ending(word, "ch") || Ending(word, "ss") ||
                Ending(word, "x") || Ending(word, "z") || Ending(word, "o"))) return word + "es";
            if (!third && Ending(word, "e"))return word + "d";
            return word + (third ? "s" : "ed");
        }
        std::string Irregular(std::string_view word, VerbForm form)
        {
            struct Row { std::string_view base, third, past, ing; };
            constexpr Row items[]{
                {"be","is","was","being"},{"have","has","had","having"},
                {"do","does","did","doing"},{"go","goes","went","going"},
                {"say","says","said","saying"},{"make","makes","made","making"},
                {"give","gives","gave","giving"},{"take","takes","took","taking"},
                {"see","sees","saw","seeing"},{"know","knows","knew","knowing"},
                {"run","runs","ran","running"},{"write","writes","wrote","writing"},
                {"read","reads","read","reading"},{"get","gets","got","getting"},
                {"come","comes","came","coming"},{"think","thinks","thought","thinking"},
                {"eat","eats","ate","eating"},{"buy","buys","bought","buying"},
                {"bring","brings","brought","bringing"},{"tell","tells","told","telling"},
                {"find","finds","found","finding"},{"leave","leaves","left","leaving"},
                {"feel","feels","felt","feeling"},{"meet","meets","met","meeting"},
                {"keep","keeps","kept","keeping"},{"put","puts","put","putting"},
                {"set","sets","set","setting"},{"cut","cuts","cut","cutting"},
                {"begin","begins","began","beginning"},{"win","wins","won","winning"},
                {"sit","sits","sat","sitting"},{"swim","swims","swam","swimming"},
                {"stop","stops","stopped","stopping"},{"drop","drops","dropped","dropping"},
                {"plan","plans","planned","planning"},{"admit","admits","admitted","admitting"},
                {"prefer","prefers","preferred","preferring"},{"occur","occurs","occurred","occurring"},
                {"travel","travels","traveled","traveling"}
            };
            for (const auto& item : items) if (item.base == word)
                return std::string(form == VerbForm::ThirdPersonSingular ? item.third :
                    form == VerbForm::Past ? item.past : item.ing);
            return {};
        }
    }

    const OutputNode& Build()
    {
        auto exe = ExecutableFolder();
        if (exe.empty())
        {
            Cache = OutputNode{}; Cache.error = L"Cannot find executable folder.";
            return Cache;
        }
        // Find the existing project assets from the executable's ancestors.
        // The Desktop's shell location is never assumed to be OneDrive.
        auto folder = exe;
        for (std::size_t depth = 0; depth < 12; ++depth)
        {
            const auto source = folder / "SAO_I_E Assets" / "Alphabet" / "Definitions";
            std::error_code ec;
            if (std::filesystem::is_directory(source, ec))
                return Build(source / "SAO_Definitions_Words.tsv",
                    source / "SAO_Word_Proper_Usage.tsv",
                    exe / "Cache" / "SAO_Word_Proper_Math");
            if (folder == folder.root_path() || folder == folder.parent_path()) break;
            folder = folder.parent_path();
        }
        Cache = OutputNode{};
        Cache.error = L"Cannot locate SAO_I_E Assets\\Alphabet\\Word Proper beside the project.";
        return Cache;
    }
    const OutputNode& Build(const std::filesystem::path& wordsFile,
        const std::filesystem::path& usageFile,
        const std::filesystem::path& cacheFolder)
    {
        Cache = OutputNode{}; g_words.clear(); g_uses.clear();
        g_sortedWordIds.clear(); g_third_person_forms.clear();
        std::string wordsText, usageText;
        if (!LoadText(wordsFile, wordsText) || !LoadText(usageFile, usageText))
        {
            Cache.error = L"Cannot read Word Proper Words/Usage TSV.";
            return Cache;
        }
        const auto sourceHash = Hash(usageText, Hash(std::string_view("\0", 1), Hash(wordsText)));
        std::error_code ec;
        std::filesystem::create_directories(cacheFolder, ec);
        if (ec)
        {
            Cache.error = L"Cannot create Word Proper cache folder.";
            return Cache;
        }
        const auto path = cacheFolder / "SAO_Word_Proper.cache";
        std::string encoded;
        bool reused = LoadText(path, encoded) && Decode(encoded, sourceHash, g_words, g_uses);
        if (!reused)
        {
            g_words.clear(); g_uses.clear();
            if (!Parse(wordsText, usageText, g_words, g_uses))
            {
                Cache.error = L"Invalid Word Proper source TSV format or IDs.";
                return Cache;
            }
            encoded = Encode(g_words, g_uses, sourceHash);
            std::vector<WordIndex> verifyWords; std::vector<Usage> verifyUses;
            if (!Save(path, encoded) || !Decode(encoded, sourceHash, verifyWords, verifyUses))
            {
                Cache.error = L"Could not write/verify Word Proper cache.";
                return Cache;
            }
        }
        if (!SortLookup())
        {
            Cache.error = L"Word Proper words contain duplicate spellings.";
            Cache = OutputNode{};
            Cache.error = L"Word Proper words contain duplicate spellings.";
            g_words.clear(); g_uses.clear(); g_sortedWordIds.clear();
            return Cache;
        }
        Cache.cache.name = L"SAO_Word_Proper.cache";
        Cache.cache.path = path.wstring();
        Cache.cache.wordCount = static_cast<std::uint32_t>(g_words.size());
        Cache.cache.usageCount = static_cast<std::uint32_t>(g_uses.size());
        Cache.cache.bytes = static_cast<std::uint32_t>(encoded.size());
        Cache.cache.sourceHash = sourceHash;
        Cache.cache.ready = true; Cache.ready = true;
        // Calculate the fixed present-tense form index once, never per sentence.
        for (const auto& word : g_words)
            if (Is(&word, Part::Verb) && !std::any_of(
                g_uses.begin() + word.first, g_uses.begin() + word.first + word.count,
                [](const Usage& use) {
                    return use.part == Part::Verb &&
                        (use.features & (ThirdSingularVerb | PastVerb | ParticipleVerb));
                }))
            {
                auto inflected = Inflect(word.spelling, VerbForm::ThirdPersonSingular);
                if (!inflected.empty()) g_third_person_forms.push_back(std::move(inflected));
            }
                std::sort(g_third_person_forms.begin(), g_third_person_forms.end());
                g_third_person_forms.erase(
                    std::unique(g_third_person_forms.begin(), g_third_person_forms.end()),
                    g_third_person_forms.end());
                return Cache;
    }
    WordResult Find(std::string_view input)
    {
        WordResult result{};
        const auto* word = Index(input);
        if (!word) return result;
        result.wordId = word->id; result.word = word->spelling; result.found = true;
        result.uses.assign(g_uses.begin() + word->first, g_uses.begin() + word->first + word->count);
        return result;
    }
    Result CheckPair(std::string_view left, std::string_view right, Relation relation)
    {
        const auto* first = Index(left); const auto* second = Index(right);
        if (!first || !second) return Result::Undetermined;
        switch (relation)
        {
        case Relation::DeterminerToNoun:
            if (!Is(first, Part::Article) && !Is(first, Part::Determiner))return Result::Undetermined;
            if (Is(second, Part::Noun) || Is(second, Part::Adjective))
            {
                if (first->spelling == "a" || first->spelling == "an")return CheckArticle(left, right);
                return Result::Allowed;
            }
            return Result::Rejected;
        case Relation::AdjectiveToNoun:
            if (!Is(first, Part::Adjective))return Result::Undetermined;
            return Is(second, Part::Noun) ? Result::Allowed : Result::Rejected;
        case Relation::PrepositionToObject:
            if (!Is(first, Part::Preposition))return Result::Undetermined;
            return IsObject(second) ? Result::Allowed : Result::Rejected;
        case Relation::ModalToBaseVerb:
            if (!Is(first, Part::Modal))return Result::Undetermined;
            if (Is(second, Part::Verb) || second->spelling == "be" ||
                second->spelling == "have" || second->spelling == "do")return Result::Allowed;
            return Result::Rejected;
        case Relation::InfinitiveToBaseVerb:
            if (first->spelling != "to")return Result::Undetermined;
            if (Is(second, Part::Verb) || second->spelling == "be" ||
                second->spelling == "have" || second->spelling == "do")return Result::Allowed;
            return Result::Rejected;
        case Relation::TransitiveVerbToObject:
            if (!Is(first, Part::Verb))return Result::Undetermined;
            if (HasFrame(first, Frame::VerbIntransitive) && !HasFrame(first, Frame::VerbFlexible))
                return Result::Rejected;
            if (HasFrame(first, Frame::VerbUnspecified))return Result::Undetermined;
            return IsObject(second) ? Result::Allowed : Result::Rejected;
        }
        return Result::Undetermined;
    }
    Result CheckArticle(std::string_view article, std::string_view nextWord)
    {
        if (!Cache.ready) return Result::Undetermined;
        const std::string a = Lower(article), word = Lower(nextWord);
        if ((a != "a" && a != "an") || word.empty())return Result::Undetermined;
        if (!Index(word))return Result::Undetermined;
        // Explicit US English vowel-sound exceptions. Unknown pronunciations are
        // not guessed via probability; all other ordinary words use initial letter.
        bool vowel = IsVowel(word[0]);
        for (const auto& prefix : { "hour","honest","honor","heir","herb" })
            if (word.rfind(prefix, 0) == 0) vowel = true;
        for (const auto& prefix : { "university","universe","unit","union","unique","use","user","usual","europe","european","eulogy","one","once" })
            if (word.rfind(prefix, 0) == 0)vowel = false;
        return (a == "an") == vowel ? Result::Allowed : Result::Rejected;
    }
    std::string Inflect(std::string_view base, VerbForm form)
    {
        const std::string b = Lower(base);
        const auto* word = Index(b);
        if (!word || (!Is(word, Part::Verb) && !Is(word, Part::Auxiliary)))return {};
        if (form == VerbForm::Base)return b;
        const auto irregular = Irregular(b, form);
        if (!irregular.empty())return irregular;
        if (form == VerbForm::ThirdPersonSingular)return PastOrThird(b, true);
        if (form == VerbForm::Past)return PastOrThird(b, false);
        if (form == VerbForm::Ing)
        {
            if (Ending(b, "ie"))return b.substr(0, b.size() - 2) + "ying";
            if (Ending(b, "e") && !Ending(b, "ee") && !Ending(b, "ye"))return b.substr(0, b.size() - 1) + "ing";
            return b + "ing";
        }
        return {};
    }
    Result CheckSubjectVerb(std::string_view subject, std::string_view verb)
    {
        const auto* sub = Index(subject);
        if (!sub || !Cache.ready)return Result::Undetermined;
        bool subjectSingular = false;
        const auto& s = sub->spelling;
        if (s == "i" || s == "me" || s == "we" || s == "us" || s == "you" || s == "they" || s == "them")
            subjectSingular = false;
        else if (s == "he" || s == "she" || s == "it") subjectSingular = true;
        else if (Is(sub, Part::Noun))
        {
            subjectSingular = true;
            for (std::uint32_t i = sub->first; i < sub->first + sub->count; ++i)
                if (g_uses[i].part == Part::Noun && (g_uses[i].features & Plural))
                    subjectSingular = false;
        }
        else return Result::Undetermined;
        if (s == "me" || s == "us" || s == "him" || s == "them")return Result::Rejected;
        const std::string v = Lower(verb);
        if (v == "am")return s == "i" ? Result::Allowed : Result::Rejected;
        if (v == "was")return s == "i" || subjectSingular ? Result::Allowed : Result::Rejected;
        if (v == "were")return s == "i" || subjectSingular ? Result::Rejected : Result::Allowed;
        if (v == "is")return subjectSingular ? Result::Allowed : Result::Rejected;
        if (v == "are")return s == "i" || subjectSingular ? Result::Rejected : Result::Allowed;
        if (v == "has" || v == "does")return subjectSingular ? Result::Allowed : Result::Rejected;
        if (v == "have" || v == "do")return subjectSingular ? Result::Rejected : Result::Allowed;
        // Modal subjects don't agree by number; only the subject-form rule applies.
        if (Is(Index(v), Part::Modal))return Result::Allowed;
        // Check a recognized regular/irregular PRESENT verb, not arbitrary words.
        if (Is(Index(v), Part::Verb))return subjectSingular ? Result::Rejected : Result::Allowed;
        if (std::binary_search(g_third_person_forms.begin(), g_third_person_forms.end(), v))
            return subjectSingular ? Result::Allowed : Result::Rejected;
        return Result::Undetermined;
    }
}

