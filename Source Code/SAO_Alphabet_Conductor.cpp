#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_Alphabet_Conductor.h"
#ifdef _WIN32
#include "SAO_Toth.h"
#endif
#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <limits>
#include <string_view>
#include <mutex>
#include <cwchar>
#include <cctype>
#ifdef _WIN32
#include <windows.h>
#endif

namespace SAO_Alphabet_Conductor
{
    OutputNode Cache{};
    namespace
    {
        namespace fs = std::filesystem;
        constexpr std::uint64_t FNV0 = 14695981039346656037ull;
        constexpr std::uint64_t FNVP = 1099511628211ull;
        constexpr std::uint64_t MAX_BITMAP_BYTES = 512ull * 1024 * 1024;
        fs::path folder;
        fs::path ruleFile, definitionFile, usageFile;
        std::mutex languageGate;
        void* languageWindow = nullptr;
        struct Entry { std::string word, definition; std::uint32_t part = 0; };
        std::vector<Entry> englishIndex;
        std::wstring userName, assistantName = L"Toth";
        std::wstring pendingQuestion;
        bool awaitingNewName = false;

        void Add32(std::string& out, std::uint32_t value)
        {
            for (unsigned n = 0; n != 4; ++n) out.push_back(char((value >> (n * 8)) & 255));
        }
        void Add64(std::string& out, std::uint64_t value)
        {
            for (unsigned n = 0; n != 8; ++n) out.push_back(char((value >> (n * 8)) & 255));
        }
        void AddUtf16(std::string& out, std::wstring_view text)
        {
            Add32(out, static_cast<std::uint32_t>(text.size()));
            for (wchar_t c : text)
            {
                const std::uint32_t u = static_cast<std::uint32_t>(c);
                Add32(out, u);  // Exact UTF-16 code units on Windows; Unicode scalar on Linux tests.
            }
        }
        void AddString(std::string& out, std::string_view text)
        {
            Add32(out, static_cast<std::uint32_t>(text.size()));
            out.append(text.data(), text.size());
        }
        std::uint64_t Hash(std::string_view data)
        {
            std::uint64_t hash = FNV0;
            for (unsigned char c : data) { hash ^= c; hash *= FNVP; }
            return hash;
        }
        std::string WideASCII(const std::wstring& word)
        {
            std::string ascii;
            for (wchar_t c : word)
            {
                if (c >= L'A' && c <= L'Z') ascii.push_back(char(c - L'A' + 'a'));
                else if (c >= L'a' && c <= L'z') ascii.push_back(char(c));
                else return {};
            }
            return ascii;
        }
        bool IsSize(Size size)
        {
            return size == Size::Small || size == Size::Medium || size == Size::Large;
        }
        bool IsDirection(Orientation orientation)
        {
            return orientation == Orientation::Left || orientation == Orientation::Right;
        }
        bool IsFile(const fs::path& path)
        {
            std::error_code ec;
            return fs::is_regular_file(path, ec);
        }
        std::uint64_t Stamp(const fs::path& path)
        {
            std::error_code ec;
            const auto result = fs::last_write_time(path, ec);
            if (ec) return 0;
            return static_cast<std::uint64_t>(result.time_since_epoch().count());
        }
        bool SizeMatches(const fs::path& path, std::uint64_t bytes)
        {
            std::error_code ec;
            return fs::is_regular_file(path, ec) && !ec && fs::file_size(path, ec) == bytes && !ec;
        }
        void FileDependency(std::string& output, const fs::path& path)
        {
            AddString(output, path.u8string());
            Add64(output, Stamp(path));
            std::error_code ec;
            Add64(output, fs::file_size(path, ec));
        }
        bool Store(const fs::path& path, std::string_view bytes)
        {
            const fs::path temporary = path.wstring() + L".tmp";
            {
                std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
                if (!out || !out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()))) return false;
                if (!out.flush()) return false;
            }
            std::error_code ec;
            fs::rename(temporary, path, ec);
            if (!ec) return true;
            fs::remove(path, ec);
            ec.clear(); fs::rename(temporary, path, ec);
            if (ec) { fs::remove(temporary); return false; }
            return true;
        }
        bool Matches(const fs::path& path, std::string_view bytes)
        {
            if (!SizeMatches(path, bytes.size())) return false;
            std::ifstream in(path, std::ios::binary);
            std::array<char, 8192> buffer{};
            std::size_t offset = 0;
            while (in && offset < bytes.size())
            {
                const auto count = (std::min)(buffer.size(), bytes.size() - offset);
                in.read(buffer.data(), static_cast<std::streamsize>(count));
                if (in.gcount() != static_cast<std::streamsize>(count) ||
                    !std::equal(buffer.data(), buffer.data() + count, bytes.data() + offset)) return false;
                offset += count;
            }
            return offset == bytes.size();
        }
        bool Copy(const fs::path& from, const fs::path& to)
        {
            std::error_code ec;
            const fs::path temporary = to.wstring() + L".tmp";
            fs::copy_file(from, temporary, fs::copy_options::overwrite_existing, ec);
            if (ec) return false;
            fs::rename(temporary, to, ec);
            if (!ec) return true;
            fs::remove(to, ec);
            ec.clear(); fs::rename(temporary, to, ec);
            if (ec) { fs::remove(temporary); return false; }
            return true;
        }
        struct Files
        {
            fs::path group, bitmap;
            bool reused = false;
        };
        Files Names(std::string_view descriptor, const wchar_t* prefix, std::uint64_t bitmapSize)
        {
            const auto key = std::to_wstring(Hash(descriptor));
            for (unsigned collision = 0; collision < 1024; ++collision)
            {
                const fs::path base = folder / (std::wstring(prefix) + key + L"_" + std::to_wstring(collision));
                const fs::path group = base.wstring() + L".cache";
                const fs::path image = base.wstring() + L".bitmap.cache";
                if (Matches(group, descriptor))
                    return { group, image, SizeMatches(image, bitmapSize) };
                if (!IsFile(group)) return { group, image, false };
            }
            return {};
        }
        bool BitmapOK(const SAO_UP_LOW_SYM_PUN_NUM_Conductor::BitmapCacheNode& bitmap)
        {
            return bitmap.ready && bitmap.width && bitmap.height &&
                bitmap.bytes == std::uint64_t(bitmap.width) * bitmap.height * 4 &&
                SizeMatches(fs::path(bitmap.path), bitmap.bytes);
        }
        // When the preceding conductor supplied an oriented composite,
        // verify its exact 16-bit request metadata, not merely the hash name.
        bool CorrectSourceText(const SAO_UP_LOW_SYM_PUN_NUM_Conductor::BitmapCacheNode& source,
            Size size, Orientation orientation, const std::wstring& text)
        {
            if (source.name.rfind(L"SAO_Oriented_", 0) != 0) return true;
            std::wstring meta = source.path;
            if (meta.size() < 6 || meta.substr(meta.size() - 6) != L".cache") return false;
            meta.replace(meta.size() - 6, 6, L".meta");
            std::ifstream in(fs::path(meta), std::ios::binary);
            if (!in) return false;
            const auto read16 = [&in](std::uint16_t& value) -> bool {
                unsigned char a = 0, b = 0;
                char bytes[2]{};
                if (!in.read(bytes, 2)) return false;
                a = static_cast<unsigned char>(bytes[0]);
                b = static_cast<unsigned char>(bytes[1]);
                value = static_cast<std::uint16_t>(a | (std::uint16_t(b) << 8));
                return true;
            };
            const auto read32 = [&read16](std::uint32_t& value) -> bool {
                std::uint16_t a = 0, b = 0;
                if (!read16(a) || !read16(b)) return false;
                value = a | (std::uint32_t(b) << 16);
                return true;
            };
            std::uint32_t count = 0;
            std::uint16_t storedSize = 0, storedOrientation = 0;
            if (!read32(count) || !read16(storedSize) || !read16(storedOrientation) ||
                count != text.size() || storedSize != static_cast<std::uint16_t>(size) ||
                storedOrientation != static_cast<std::uint16_t>(orientation)) return false;
            for (wchar_t c : text)
            {
                std::uint16_t stored = 0;
                if (!read16(stored) || stored != static_cast<std::uint16_t>(c)) return false;
            }
            return in.peek() == std::char_traits<char>::eof();
        }

        void SetError(const std::wstring& error) { Cache.error = error; }

        // From an OWNED set of word bitmaps, write another OWNED bitmap.
        // Row-streamed, not a full-frame allocation.
        bool Join(const std::vector<WordNode>& words, Orientation orientation,
            std::uint32_t spacePixels, const fs::path& destination,
            std::uint32_t& outputWidth, std::uint32_t& outputHeight,
            std::uint64_t& outputBytes)
        {
            std::vector<std::size_t> order;
            for (std::size_t i = 0; i < words.size(); ++i) order.push_back(i);
            if (orientation == Orientation::Right) std::reverse(order.begin(), order.end());

            std::uint64_t width = 0, height = 0;
            for (std::size_t j = 0; j < order.size(); ++j)
            {
                const auto& word = words[order[j]];
                if (!word.bitmap.cache.ready || !SizeMatches(word.bitmap.cache.path, word.bitmap.cache.bytes)) return false;
                const bool close = !word.text.empty() &&
                    std::wstring_view(L".,!?;:)]}").find(word.text[0]) != std::wstring_view::npos;
                const bool previousOpen = j && !words[order[j - 1]].text.empty() &&
                    std::wstring_view(L"([{").find(words[order[j - 1]].text.back()) != std::wstring_view::npos;
                if (j && !close && !previousOpen) width += spacePixels;
                width += word.bitmap.width;
                height = (std::max)(height, std::uint64_t(word.bitmap.height));
            }
            if (!width || !height || width > (std::numeric_limits<std::uint32_t>::max)() / 4 ||
                height > (std::numeric_limits<std::uint32_t>::max)() ||
                width * height * 4 > MAX_BITMAP_BYTES) return false;

            const fs::path temp = destination.wstring() + L".tmp";
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out) return false;
            std::vector<std::ifstream> inputs;
            inputs.reserve(order.size());
            for (std::size_t index : order)
            {
                inputs.emplace_back(fs::path(words[index].bitmap.cache.path), std::ios::binary);
                if (!inputs.back()) return false;
            }
            std::vector<char> buffer(static_cast<std::size_t>(width * 4), 0);
            for (std::uint64_t y = 0; y < height; ++y)
            {
                std::fill(buffer.begin(), buffer.end(), 0);
                std::size_t offset = 0;
                for (std::size_t j = 0; j < order.size(); ++j)
                {
                    const auto& word = words[order[j]];
                    const bool close = !word.text.empty() &&
                        std::wstring_view(L".,!?;:)]}").find(word.text[0]) != std::wstring_view::npos;
                    const bool previousOpen = j && !words[order[j - 1]].text.empty() &&
                        std::wstring_view(L"([{").find(words[order[j - 1]].text.back()) != std::wstring_view::npos;
                    if (j && !close && !previousOpen) offset += spacePixels * 4;
                    const std::size_t span = std::size_t(word.bitmap.width) * 4;
                    if (y < word.bitmap.height)
                    {
                        inputs[j].read(buffer.data() + offset, static_cast<std::streamsize>(span));
                        if (inputs[j].gcount() != static_cast<std::streamsize>(span)) return false;
                    }
                    offset += span;
                }
                out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                if (!out) return false;
            }
            out.close();
            if (!out) return false;
            std::error_code ec;
            fs::rename(temp, destination, ec);
            if (ec)
            {
                fs::remove(destination, ec); ec.clear();
                fs::rename(temp, destination, ec);
            }
            if (ec) return false;
            outputWidth = static_cast<std::uint32_t>(width);
            outputHeight = static_cast<std::uint32_t>(height);
            outputBytes = width * height * 4;
            return true;
        }

        Check FromProper(SAO_Word_Proper_Math::Result result)
        {
            switch (result)
            {
            case SAO_Word_Proper_Math::Result::Allowed: return Check::Allowed;
            case SAO_Word_Proper_Math::Result::Rejected: return Check::Rejected;
            default: return Check::Undetermined;
            }
        }
        bool StartsWithAsciiLetter(const std::wstring& text)
        {
            return !text.empty() && ((text.front() >= L'A' && text.front() <= L'Z') ||
                (text.front() >= L'a' && text.front() <= L'z'));
        }
        bool IsTerminal(const std::wstring& word)
        {
            return word == L"." || word == L"?" || word == L"!";
        }
        void AddRule(std::vector<RuleCheckNode>& rules, std::uint16_t id,
            std::uint32_t first, std::uint32_t second, Check result)
        {
            if (SAO_English_Rules_Math::Find(id))
                rules.push_back({ id, first, second, result });
        }
    }

    const OutputNode& BuildAt(const fs::path& cacheFolder,
        const SAO_English_Rules_Math::OutputNode& rules,
        const SAO_Definitions_Math::OutputNode& definitions,
        const SAO_Word_Proper_Math::OutputNode& usage,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::OutputNode& characters)
    {
        std::lock_guard<std::mutex> languageLock(languageGate);
        const std::wstring typed = Cache.inputText;
        const std::wstring forwardedCommand = Cache.submittedCommand;
        const std::wstring previousToth = Cache.tothText;
        const std::wstring previousBastet = Cache.bastetText;
        const auto previousRevision = Cache.revision;
        const auto previousAnswerRevision = Cache.answerRevision;
        Cache = {};
        Cache.inputText = typed;
        Cache.submittedCommand = forwardedCommand;
        Cache.tothText = previousToth;
        Cache.bastetText = previousBastet;
        Cache.revision = previousRevision;
        Cache.answerRevision = previousAnswerRevision;
        if (!rules.ready || !rules.rules.ready || !definitions.ready || !definitions.cache.ready ||
            !usage.ready || !usage.cache.ready || !characters.ready)
        {
            Cache.error = L"An input Rules, Definitions, Word Proper, or character conductor cache is not ready.";
            return Cache;
        }
        ruleFile = fs::path(rules.rules.path);
        definitionFile = fs::path(definitions.cache.path);
        usageFile = fs::path(usage.cache.path);
        if (!IsFile(ruleFile) || !IsFile(definitionFile) || !IsFile(usageFile))
        {
            Cache.error = L"An input English rules/definition/usage cache file is missing.";
            return Cache;
        }
        folder = cacheFolder;
        std::error_code ec;
        fs::create_directories(folder, ec);
        if (ec || !fs::is_directory(folder))
        {
            Cache.error = L"Cannot create Alphabet Conductor cache folder.";
            return Cache;
        }
        Cache.folder = folder.wstring();
        Cache.definitionsPath = definitionFile.wstring();
        Cache.usagePath = usageFile.wstring();
        Cache.ready = true;
        return Cache;
    }

    const OutputNode& Build(const SAO_English_Rules_Math::OutputNode& rules,
        const SAO_Definitions_Math::OutputNode& definitions,
        const SAO_Word_Proper_Math::OutputNode& usage,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::OutputNode& characters)
    {
        fs::path base;
#ifdef _WIN32
        wchar_t executable[MAX_PATH]{};
        const DWORD n = GetModuleFileNameW(nullptr, executable, MAX_PATH);
        if (!n || n >= MAX_PATH) { Cache = {}; Cache.error = L"Cannot locate executable."; return Cache; }
        base = fs::path(std::wstring(executable, n)).parent_path();
#else
        base = fs::current_path();
#endif
        return BuildAt(base / L"Cache" / L"SAO_Alphabet_Conductor", rules, definitions, usage, characters);
    }

    WordNode Word(const std::vector<CharacterOperation>& operations,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::BitmapCacheNode& source,
        Size size, Orientation orientation)
    {
        WordNode result{};
        if (!Cache.ready || !IsSize(size) || !IsDirection(orientation) || operations.empty()) return result;
        result.size = size;
        result.orientation = orientation;
        for (const auto& step : operations)
        {
            if (step.polarity == Polarity::Plus)
                result.text.push_back(step.character);
            else if (step.polarity == Polarity::Minus)
            {
                auto found = result.text.find_last_of(step.character);
                if (found == std::wstring::npos)
                {
                    SetError(L"Cannot subtract a character that is not in the word grouping.");
                    return result;
                }
                result.text.erase(found, 1);
            }
            else return result;
        }
        if (result.text.empty() || result.text.size() > 256) return result;
        // Input is already bitmap-cached by previous conductor. No back-call.
        if (!BitmapOK(source) || source.orientation != orientation ||
            !CorrectSourceText(source, size, orientation, result.text))
        {
            SetError(L"Cannot obtain source character bitmap cache for word.");
            return result;
        }
        const std::string ascii = WideASCII(result.text);
        if (!ascii.empty())
        {
            const auto definition = SAO_Definitions_Math::Find(ascii);
            const auto usage = SAO_Word_Proper_Math::Find(ascii);
            if (definition.found)
            {
                result.known = true;
                result.wordId = definition.id;
                for (const auto& meaning : definition.meanings)
                    result.senseIds.push_back(meaning.id);
            }
            if (usage.found && usage.wordId == result.wordId)
                result.usages = usage.uses;
        }
        std::string record = "SAOALPWORD01";
        Add32(record, static_cast<std::uint32_t>(size));
        Add32(record, static_cast<std::uint32_t>(orientation));
        AddUtf16(record, result.text);
        Add32(record, result.wordId);
        Add32(record, static_cast<std::uint32_t>(result.senseIds.size()));
        for (auto id : result.senseIds) Add32(record, id);
        Add32(record, static_cast<std::uint32_t>(result.usages.size()));
        for (const auto& use : result.usages)
        {
            Add32(record, use.senseId); Add32(record, use.wordId);
            Add32(record, static_cast<std::uint32_t>(use.part));
            Add32(record, static_cast<std::uint32_t>(use.frame));
            Add32(record, use.features); Add32(record, use.curated ? 1 : 0);
        }
        FileDependency(record, ruleFile);
        FileDependency(record, definitionFile);
        FileDependency(record, usageFile);
        FileDependency(record, fs::path(source.path));
        const Files files = Names(record, L"SAO_Word_", source.bytes);
        if (files.group.empty()) return result;
        if (!files.reused && !Copy(fs::path(source.path), files.bitmap))
        {
            SetError(L"Cannot copy word bitmap into Alphabet-owned cache.");
            return result;
        }
        if (!Matches(files.group, record) && !Store(files.group, record))
        {
            SetError(L"Cannot store word grouping cache.");
            return result;
        }
        result.bitmap.cache = { files.bitmap.wstring(), source.bytes, SizeMatches(files.bitmap, source.bytes) };
        result.bitmap.width = source.width;
        result.bitmap.height = source.height;
        result.group = { files.group.wstring(), record.size(), Matches(files.group, record) };
        result.ready = result.group.ready && result.bitmap.cache.ready;
        return result;
    }

    WordNode Word(const std::wstring& text,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::BitmapCacheNode& source,
        Size size, Orientation orientation)
    {
        std::vector<CharacterOperation> operations;
        operations.reserve(text.size());
        for (auto c : text) operations.push_back({ c, Polarity::Plus });
        return Word(operations, source, size, orientation);
    }

    SentenceNode Sentence(const std::vector<WordOperation>& operations,
        Size size, Orientation orientation, std::uint32_t spacePixels, bool composeBitmap)
    {
        SentenceNode output{};
        if (!Cache.ready || !IsSize(size) || !IsDirection(orientation) || operations.empty()) return output;
        std::vector<std::wstring> tokens;
        for (const auto& operation : operations)
        {
            const auto& word = operation.word;
            if (word.text.empty() || word.text.size() > 256) return output;
            if (operation.polarity == Polarity::Plus)
            {
                if (!word.ready || word.size != size || word.orientation != orientation ||
                    !word.group.ready || !word.bitmap.cache.ready ||
                    !SizeMatches(fs::path(word.group.path), word.group.bytes) ||
                    !SizeMatches(fs::path(word.bitmap.cache.path), word.bitmap.cache.bytes))
                {
                    SetError(L"A forward word grouping cache is missing or incompatible.");
                    return output;
                }
                tokens.push_back(word.text);
                output.words.push_back(word);
            }
            else if (operation.polarity == Polarity::Minus)
            {
                auto it = std::find(tokens.rbegin(), tokens.rend(), word.text);
                if (it == tokens.rend())
                {
                    SetError(L"Cannot subtract a word absent from sentence grouping.");
                    return output;
                }
                const auto index = static_cast<std::size_t>(std::distance(tokens.begin(), (it + 1).base()));
                tokens.erase(tokens.begin() + index);
                output.words.erase(output.words.begin() + index);
            }
            else return output;
        }
        if (tokens.empty() || tokens.size() > 128) return output;
        for (std::size_t i = 0; i < tokens.size(); ++i)
        {
            const bool close = !tokens[i].empty() && std::wstring_view(L".,!?;:)]}").find(tokens[i].front()) != std::wstring_view::npos;
            const bool previousOpen = i && !tokens[i - 1].empty() && std::wstring_view(L"([{").find(tokens[i - 1].back()) != std::wstring_view::npos;
            if (i && !close && !previousOpen) output.text += L' ';
            output.text += tokens[i];
        }
        if (StartsWithAsciiLetter(output.text))
            AddRule(output.rules, 14, 0, 0,
                output.text.front() >= L'A' && output.text.front() <= L'Z' ? Check::Allowed : Check::Rejected);
        for (std::size_t i = 0; i < tokens.size(); ++i)
        {
            if (tokens[i] == L"i" || tokens[i] == L"I")
                AddRule(output.rules, 15, static_cast<std::uint32_t>(i),
                    static_cast<std::uint32_t>(i), tokens[i] == L"I" ? Check::Allowed : Check::Rejected);
            if (i && !tokens[i].empty() &&
                std::wstring_view(L".,!?;:").find(tokens[i].front()) != std::wstring_view::npos)
                AddRule(output.rules, 90, static_cast<std::uint32_t>(i - 1),
                    static_cast<std::uint32_t>(i), Check::Allowed);
        }
        if (!tokens.empty())
        {
            const std::wstring & final = tokens.back();
            AddRule(output.rules, final == L"?" ? 53 : final == L"!" ? 55 : 52,
                static_cast<std::uint32_t>(tokens.size() - 1),
                static_cast<std::uint32_t>(tokens.size() - 1),
                IsTerminal(final) ? Check::Allowed : Check::Rejected);
        }
        // Only check relationships the Word Proper source can actually classify.
        for (std::size_t i = 1; i < tokens.size(); ++i)
        {
            const auto prior = WideASCII(tokens[i - 1]);
            const auto next = WideASCII(tokens[i]);
            if (prior.empty() || next.empty()) continue;
            if (prior == "a" || prior == "an")
            {
                AddRule(output.rules, 100, static_cast<std::uint32_t>(i - 1),
                    static_cast<std::uint32_t>(i), FromProper(SAO_Word_Proper_Math::CheckArticle(prior, next)));
            }
            const auto usage = SAO_Word_Proper_Math::Find(prior);
            bool modal = false;
            for (const auto& use : usage.uses)
                if (use.part == SAO_Word_Proper_Math::Part::Modal) modal = true;
            if (modal)
            {
                AddRule(output.rules, 118, static_cast<std::uint32_t>(i - 1),
                    static_cast<std::uint32_t>(i), FromProper(SAO_Word_Proper_Math::CheckPair(
                        prior, next, SAO_Word_Proper_Math::Relation::ModalToBaseVerb)));
            }
            if (i == 1 && (prior == "i" || prior == "she" || prior == "he" ||
                prior == "it" || prior == "we" || prior == "you" || prior == "they"))
            {
                AddRule(output.rules, 110, 0, 1,
                    FromProper(SAO_Word_Proper_Math::CheckSubjectVerb(prior, next)));
            }
        }
        bool rejected = false;
        for (const auto& rule : output.rules)
        {
            if (rule.result == Check::Rejected) rejected = true;
        }
        // Pairwise findings cannot establish complete grammatical correctness.
        // Reject only a proved violation; otherwise remain undetermined.
        output.grammar = rejected ? Check::Rejected : Check::Undetermined;

        // Exact size of punctuation-aware, orientation-aware composition.
        std::vector<std::size_t> renderOrder;
        for (std::size_t i = 0; i < output.words.size(); ++i) renderOrder.push_back(i);
        if (orientation == Orientation::Right) std::reverse(renderOrder.begin(), renderOrder.end());
        std::uint64_t combinedWidth = 0, maximumHeight = 0;
        for (std::size_t j = 0; j < renderOrder.size(); ++j)
        {
            const auto& word = output.words[renderOrder[j]];
            const bool close = !word.text.empty() &&
                std::wstring_view(L".,!?;:)]}").find(word.text.front()) != std::wstring_view::npos;
            const bool previousOpen = j && !output.words[renderOrder[j - 1]].text.empty() &&
                std::wstring_view(L"([{").find(output.words[renderOrder[j - 1]].text.back()) != std::wstring_view::npos;
            if (j && !close && !previousOpen) combinedWidth += spacePixels;
            combinedWidth += word.bitmap.width;
            maximumHeight = (std::max)(maximumHeight, std::uint64_t(word.bitmap.height));
        }
        if (combinedWidth > (std::numeric_limits<std::uint32_t>::max)() / 4 ||
            maximumHeight > (std::numeric_limits<std::uint32_t>::max)() ||
            combinedWidth > (std::numeric_limits<std::uint64_t>::max)() / (maximumHeight * 4))
            return output;
        const std::uint64_t expected = combinedWidth * maximumHeight * 4;
        std::string record = "SAOALPSENT01";
        Add32(record, static_cast<std::uint32_t>(size));
        Add32(record, static_cast<std::uint32_t>(orientation));
        Add32(record, spacePixels);
        Add32(record, composeBitmap ? 1 : 0);
        AddUtf16(record, output.text);
        Add32(record, static_cast<std::uint32_t>(output.words.size()));
        for (const auto& word : output.words)
        {
            AddUtf16(record, word.text);
            Add32(record, word.wordId);
            FileDependency(record, fs::path(word.group.path));
            FileDependency(record, fs::path(word.bitmap.cache.path));
        }
        Add32(record, static_cast<std::uint32_t>(output.rules.size()));
        for (const auto& check : output.rules)
        {
            Add32(record, check.ruleId); Add32(record, check.firstWord);
            Add32(record, check.secondWord); Add32(record, static_cast<std::uint32_t>(check.result));
        }
        FileDependency(record, ruleFile);
        FileDependency(record, usageFile);
        FileDependency(record, definitionFile);
        // Actual punctuation-aware width comes from Join; cached metadata does
        // not depend on a guessed width when metadata-only grouping is selected.
        const Files files = Names(record, L"SAO_Sentence_", composeBitmap ? expected : 0);
        if (files.group.empty()) return output;
        if (composeBitmap)
        {
            if (!files.reused)
            {
                std::uint32_t w = 0, h = 0;
                std::uint64_t bytes = 0;
                if (!Join(output.words, orientation, spacePixels, files.bitmap, w, h, bytes))
                {
                    SetError(L"Cannot compose sentence bitmap (check 512 MiB bitmap limit).");
                    return output;
                }
                output.bitmap.width = w;
                output.bitmap.height = h;
                output.bitmap.cache = { files.bitmap.wstring(), bytes, SizeMatches(files.bitmap, bytes) };
            }
            else
            {
                output.bitmap.width = static_cast<std::uint32_t>(combinedWidth);
                output.bitmap.height = static_cast<std::uint32_t>(maximumHeight);
                output.bitmap.cache = { files.bitmap.wstring(), expected,
                    SizeMatches(files.bitmap, expected) };
            }
        }
        if (!Matches(files.group, record) && !Store(files.group, record))
        {
            SetError(L"Cannot store sentence grouping cache.");
            return output;
        }
        output.group = { files.group.wstring(), record.size(), Matches(files.group, record) };
        output.ready = output.group.ready && (!composeBitmap || output.bitmap.cache.ready);
        return output;
    }
}


// All interaction enters this completed Alphabet stage. It reads its OWN
// upstream cache references; it NEVER calls SAO_Definitions_Math::Find(),
// SAO_Word_Proper_Math::Find(), or any preceding Build().
namespace SAO_Alphabet_Conductor
{
    namespace
    {
        std::uint32_t U32(const std::string& s, std::size_t at)
        {
            if (at > s.size() || s.size() - at < 4) return 0;
            return std::uint32_t((unsigned char)s[at]) |
                (std::uint32_t((unsigned char)s[at + 1]) << 8) |
                (std::uint32_t((unsigned char)s[at + 2]) << 16) |
                (std::uint32_t((unsigned char)s[at + 3]) << 24);
        }
        bool LoadLexicon(const std::wstring& path)
        {
            englishIndex.clear();
            std::ifstream in(fs::path(path), std::ios::binary | std::ios::ate);
            if (!in) return false;
            const auto size = in.tellg();
            if (size < 44 || size > 32 * 1024 * 1024) return false;
            std::string data(static_cast<std::size_t>(size), '\0');
            in.seekg(0);
            if (!in.read(&data[0], size) || data.compare(0, 8, "SAODEF01") != 0 ||
                U32(data, 8) != 1) return false;
            const std::uint32_t wc = U32(data, 12), sc = U32(data, 16);
            const std::uint32_t letters = U32(data, 20), glosses = U32(data, 24);
            if (!wc || !sc || wc > 100000 || sc > 1000000 ||
                44ull + std::uint64_t(wc) * 20 + std::uint64_t(sc) * 16 +
                std::uint64_t(letters) + glosses != data.size()) return false;
            const std::size_t wordTable = 44, senseTable = wordTable + std::size_t(wc) * 20;
            const std::size_t wordData = senseTable + std::size_t(sc) * 16;
            const std::size_t glossData = wordData + letters;
            for (std::uint32_t i = 0; i < wc; ++i)
            {
                const auto w = wordTable + std::size_t(i) * 20;
                const auto first = U32(data, w + 4), count = U32(data, w + 8);
                const auto offset = U32(data, w + 12), n = U32(data, w + 16);
                if (U32(data, w) != i + 1 || !count || std::uint64_t(first) + count > sc ||
                    std::uint64_t(offset) + n > letters || !n) return false;
                std::string word = data.substr(wordData + offset, n);
                const auto sp = senseTable + std::size_t(first) * 16;
                const auto pos = U32(data, sp + 4), go = U32(data, sp + 8), gn = U32(data, sp + 12);
                if (pos < 1 || pos > 10 || !gn || std::uint64_t(go) + gn > glosses) return false;
                englishIndex.push_back({ std::move(word), data.substr(glossData + go,gn),pos });
            }
            // Definitions preserves historical word IDs; appended words need not
            // be alphabetic by ID. Sort only this lookup, never the source data.
            std::sort(englishIndex.begin(), englishIndex.end(),
                [](const Entry& a, const Entry& b) { return a.word < b.word; });
            for (std::size_t i = 1; i < englishIndex.size(); ++i)
                if (englishIndex[i - 1].word == englishIndex[i].word)
                    return false;
            return true;
        }
        std::wstring LowerEnglish(std::wstring s)
        {
            for (wchar_t& c : s) if (c >= L'A' && c <= L'Z') c += 32;
            return s;
        }
        std::wstring TrimEnglish(std::wstring s)
        {
            const auto l = s.find_first_not_of(L" \t\r\n");
            if (l == std::wstring::npos) return {};
            const auto r = s.find_last_not_of(L" \t\r\n");
            return s.substr(l, r - l + 1);
        }
        bool StartsEnglish(const std::wstring& s, const wchar_t* prefix)
        {
            return s.rfind(prefix, 0) == 0;
        }
        std::wstring WideEnglish(const std::string& s)
        {
#ifdef _WIN32
            if (s.empty()) return {};
            const int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                s.data(), static_cast<int>(s.size()), nullptr, 0);
            if (n <= 0) return {};
            std::wstring result(static_cast<std::size_t>(n), L'\0');
            return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                s.data(), static_cast<int>(s.size()), &result[0], n) == n ? result : std::wstring{};
#else
            return std::wstring(s.begin(), s.end());
#endif
        }
        std::string SimpleAscii(const std::wstring& w)
        {
            std::string s;
            for (wchar_t c : LowerEnglish(w))
            {
                if (c < L'a' || c > L'z') return {};
                s.push_back(static_cast<char>(c));
            }
            return s;
        }
        std::wstring PartName(std::uint32_t part)
        {
            switch (part)
            {
            case 1: return L"noun"; case 2: return L"verb";
            case 3: return L"adjective"; case 4: return L"adverb";
            case 5: return L"pronoun"; case 6: return L"preposition";
            case 7: return L"conjunction"; case 8: return L"determiner";
            case 9: return L"article"; case 10: return L"interjection";
            default: return L"word";
            }
        }
        void Respond(const std::wstring& toth, const std::wstring& bastet)
        {
            Cache.tothText = toth; Cache.bastetText = bastet;
        }
        void SaveNames()
        {
            if (Cache.folder.empty()) return;
            std::ofstream f(fs::path(Cache.folder) / L"SAO_Alphabet_Memory.cache",
                std::ios::binary | std::ios::trunc);
            for (wchar_t c : userName) if (c >= 32 && c < 127) f.put(static_cast<char>(c));
            f.put('\n');
            for (wchar_t c : assistantName) if (c >= 32 && c < 127) f.put(static_cast<char>(c));
            f.put('\n');
        }
        void LoadNames()
        {
            if (Cache.folder.empty()) return;
            std::ifstream f(fs::path(Cache.folder) / L"SAO_Alphabet_Memory.cache",
                std::ios::binary);
            std::string a, b;
            if (f && std::getline(f, a) && std::getline(f, b))
            {
                userName = WideEnglish(a);
                if (!b.empty()) assistantName = WideEnglish(b);
            }
        }
        void ProcessEnglish(const std::wstring& typed)
        {
            std::wstring raw = TrimEnglish(typed);
            std::wstring query = LowerEnglish(raw);
            while (!query.empty() && (query.back() == L'?' || query.back() == L'.' || query.back() == L'!'))
                query.pop_back();
            query = TrimEnglish(query);
            if (query.empty()) return;
            if (!Cache.ready)
            {
                pendingQuestion = typed;  // Keep the last submitted question until ready.
                Respond(L"The alphabet cache is still preparing.",
                    L"The game can continue while language becomes ready.");
                return;
            }
            if (awaitingNewName)
            {
                assistantName = raw; awaitingNewName = false; SaveNames();
                Respond(L"My name is now " + assistantName + L".", L"Your chosen name has been saved.");
                return;
            }
            if (query == L"hello" || query == L"hi" || query == L"hey")
            {
                Respond(L"Hello" + (userName.empty() ? std::wstring() : L", " + userName) + L".",
                    L"Greetings. We can look up definitions and usage together."); return;
            }
            if (StartsEnglish(query, L"my name is "))
            {
                userName = TrimEnglish(raw.substr(11)); SaveNames();
                Respond(L"Nice to meet you, " + userName + L".", L"I'll remember your name."); return;
            }
            if (query == L"who are you" || query == L"what is your name")
            {
                Respond(L"My name is " + assistantName + L".",
                    L"And I am Bastet. I help with word usage and examples."); return;
            }
            if (query == L"rename you" || query == L"i would like to rename you")
            {
                awaitingNewName = true;
                Respond(L"What would you like to call me?", L"The chosen name can be remembered."); return;
            }
            if (query == L"save to memory")
            {
                SaveNames(); Respond(L"Memory saved.", L"Your names are preserved."); return;
            }
            std::wstring target;
            for (const wchar_t* prefix : { L"what is the definition of ",L"what's the definition of ",
                L"give me the definition of ",L"definition of ",L"define ",L"what does " })
            {
                if (StartsEnglish(query, prefix))
                {
                    target = TrimEnglish(query.substr(std::wcslen(prefix))); break;
                }
            }
            if (target.empty() && StartsEnglish(query, L"what is "))
                target = TrimEnglish(query.substr(8));
            if (target.size() > 5 && target.substr(target.size() - 5) == L" mean")
                target = TrimEnglish(target.substr(0, target.size() - 5));
            if (StartsEnglish(target, L"the word ")) target = target.substr(9);
            for (const wchar_t* article : { L"the ",L"an ",L"a " })
                if (StartsEnglish(target, article)) { target = target.substr(std::wcslen(article)); break; }
            if (target.size() >= 2 && target.front() == L'\'' && target.back() == L'\'')
                target = target.substr(1, target.size() - 2);
            const std::string lookup = SimpleAscii(target);
            if (!lookup.empty())
            {
                const auto found = std::lower_bound(englishIndex.begin(), englishIndex.end(), lookup,
                    [](const Entry& e, const std::string& q) {return e.word < q; });
                if (found != englishIndex.end() && found->word == lookup)
                {
                    std::wstring meaning = WideEnglish(found->definition);
                    if (!meaning.empty() && meaning.back() == L'.') meaning.pop_back();
                    std::wstring title = target;
                    if (!title.empty() && title.front() >= L'a' && title.front() <= L'z') title.front() -= 32;
                    std::wstring example;
                    if (lookup == "time") example = L"Example: We need more time to finish.";
                    else if (lookup == "light") example = L"Example: Light entered the room.";
                    else if (lookup == "word") example = L"Example: Each word has a meaning.";
                    else if (lookup == "learn") example = L"Example: We learn something new every day.";
                    else example = L"Usage: '" + target + L"' is a " + PartName(found->part) +
                        L". A sentence example is not cached for this word yet.";
                    Respond(title + L" is " + meaning + L".", example);
                }
                else Respond(L"The word '" + target + L"' is not in the connected vocabulary.",
                    L"We can add it to the language cache later.");
                return;
            }
            if (query.find(L"calculate") != std::wstring::npos ||
                query.find(L" + ") != std::wstring::npos ||
                query.find(L" * ") != std::wstring::npos)
            {
                Respond(L"The mathematics conductor has not been connected yet.",
                    L"The Alphabet connection is ready for the next conductor."); return;
            }
            Respond(L"I can answer definitions, greetings, and name commands for now.",
                L"Try: What is the definition of time?");
        }
        void ForwardLanguage()
        {
#ifdef _WIN32
            OutputNode snapshot;
            {
                std::lock_guard<std::mutex> lock(languageGate);
                snapshot = Cache;
            }
            SAO_Toth::ReceiveAlphabet(snapshot);
#endif
        }
    }

    void Attach(void* programHandle)
    {
        std::lock_guard<std::mutex> lock(languageGate);
        languageWindow = programHandle; // Owned by Program; never created here.
    }
    bool Character(wchar_t character)
    {
        if (character < 32 || character == 127) return false;
        {
            std::lock_guard<std::mutex> lock(languageGate);
            if (Cache.inputText.size() >= 240) return false;
            Cache.inputText.push_back(character);
            ++Cache.revision;
        }
        ForwardLanguage(); return true;
    }
    bool Backspace()
    {
        {
            std::lock_guard<std::mutex> lock(languageGate);
            if (Cache.inputText.empty()) return false;
            Cache.inputText.pop_back(); ++Cache.revision;
        }
        ForwardLanguage(); return true;
    }
    bool Submit()
    {
        {
            std::lock_guard<std::mutex> lock(languageGate);
            if (Cache.inputText.empty()) return false;
            const std::wstring question = Cache.inputText;
            // Forward every # command to Toth, preserving player-name casing.
            // Alphabet never calls Toth's profile routines or earlier Maths.
            const std::wstring rawCommand = TrimEnglish(question);
            const std::wstring command = LowerEnglish(rawCommand);
            if (!rawCommand.empty() && rawCommand.front() == L'#')
            {
                Cache.submittedCommand =
                    (command == L"#maximize" || command == L"#minimize" ||
                        command == L"#exit_game" || command == L"#player name") ?
                    command : rawCommand;
            }
            else
            {
                Cache.submittedCommand.clear();
                ProcessEnglish(question);
            }
            Cache.inputText.clear();
            ++Cache.revision;
            Cache.answerRevision = Cache.revision;
        }
        ForwardLanguage(); return true;
    }
    // Called by upstream source assembly after Build(rules,definitions,usage,chars).
    bool PrepareVocabulary()
    {
        {
            std::lock_guard<std::mutex> lock(languageGate);
            if (!Cache.ready || Cache.definitionsPath.empty() ||
                !LoadLexicon(Cache.definitionsPath))
            {
                Cache.ready = false;
                if (Cache.error.empty())
                    Cache.error = L"Cannot use the forwarded definitions cache.";
                Cache.tothText = L"The English sources are not ready.";
                Cache.bastetText = Cache.error;
            }
            else
            {
                LoadNames();
                if (!pendingQuestion.empty())
                {
                    const std::wstring question = pendingQuestion;
                    pendingQuestion.clear();
                    ProcessEnglish(question); // Once, after caches are ready.
                    Cache.answerRevision = Cache.revision + 1;
                }
                else if (Cache.tothText == L"The alphabet cache is still preparing.")
                {
                    Cache.tothText = L"Alphabet ready. Ask your question.";
                    Cache.bastetText = L"Language and word definitions are ready.";
                }
            }
            ++Cache.revision;
        }
        ForwardLanguage();
        return Cache.ready;
    }
    void SourceFailed(const std::wstring& error)
    {
        {
            std::lock_guard<std::mutex> lock(languageGate);
            Cache.ready = false;
            Cache.error = error;
            Cache.tothText = L"Alphabet source cache could not be prepared.";
            Cache.bastetText = error;
            ++Cache.revision;
        }
        ForwardLanguage();  // Error still follows Alphabet -> Toth.
    }
    void Detach()
    {
        std::lock_guard<std::mutex> lock(languageGate);
        languageWindow = nullptr;
        englishIndex.clear();
    }
}


