#pragma once

// Windows RPC headers define `small` as `char`. The SAO upstream
// conductors use `small` as an argument name; cancel the macro here,
// before including their headers. No upstream source changes.
#ifdef small
#undef small
#endif
#include "SAO_English_Rules_Math.h"
#include "SAO_Definitions_Math.h"
#include "SAO_Word_Proper_Math.h"
#include "SAO_UP_LOW_SYM_PUN_NUM_Conductor.h"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

// One directional stage before Thoth: cache -> grouping -> NEW cache.
// No PNG decoder, no Main Screen, no calls to upstream Build() functions.
namespace SAO_Alphabet_Conductor
{
    using Size = SAO_UP_LOW_SYM_PUN_NUM_Conductor::Size;
    using Orientation = SAO_UP_LOW_SYM_PUN_NUM_Conductor::Orientation;
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };
    enum class Check : std::uint8_t { Undetermined, Allowed, Rejected };

    struct CharacterOperation
    {
        wchar_t character = L'\0';
        Polarity polarity = Polarity::Plus;
    };


    struct CacheNode
    {
        std::wstring path;          // OWN .cache, never upstream path.
        std::uint64_t bytes = 0;
        bool ready = false;
    };

    struct BitmapNode
    {
        CacheNode cache;            // OWN raw 32bpp top-down premultiplied BGRA.
        std::uint32_t width = 0;
        std::uint32_t height = 0;
    };

    struct WordNode
    {
        std::wstring text;
        Size size = Size::Small;
        Orientation orientation = Orientation::Left;
        CacheNode group;            // OWN cached record: letters + word/sense/usage IDs.
        BitmapNode bitmap;          // OWN recached assembled glyph bitmap.
        std::uint32_t wordId = 0;
        std::vector<std::uint32_t> senseIds;
        std::vector<SAO_Word_Proper_Math::Usage> usages;
        bool known = false;
        bool ready = false;
    };

    struct WordOperation
    {
        WordNode word;            // Already formed AND recached in this conductor.
        Polarity polarity = Polarity::Plus;
    };

    struct RuleCheckNode
    {
        std::uint16_t ruleId = 0;  // Fixed SAO_English_Rules_Math ID.
        std::uint32_t firstWord = 0;
        std::uint32_t secondWord = 0;
        Check result = Check::Undetermined;
    };

    struct SentenceNode
    {
        CacheNode group;            // OWN cached sentence/word/rule grouping.
        BitmapNode bitmap;          // OWN recached oriented bitmap, if requested.
        std::vector<WordNode> words;
        std::vector<RuleCheckNode> rules;
        std::wstring text;          // Logical token order, never silently corrected.
        Check grammar = Check::Undetermined;
        bool ready = false;
    };

    struct OutputNode
    {
        std::wstring folder;
        std::wstring definitionsPath; // Forwarded upstream cache location.
        std::wstring usagePath;
        std::wstring error;
        std::wstring inputText;
        std::wstring submittedCommand; // Forwarded system command; empty for ordinary English.
        std::wstring tothText;
        std::wstring bastetText;
        std::uint64_t revision = 0;
        std::uint64_t answerRevision = 0;
        bool ready = false;
    };

    extern OutputNode Cache;

    // External input enters the Alphabet stage, then flows forward to Toth.
    // No Toth calls into the Alphabet, definitions, words, or character Maths.
    void Attach(void* programHandle);
    bool Character(wchar_t character);
    bool Backspace();
    bool Submit();
    bool PrepareVocabulary();
    void SourceFailed(const std::wstring& error); // Upstream failure forwarded, never polled.
    void Detach();

    // Four independent upstream results arrive forward, already built.
    const OutputNode& Build(
        const SAO_English_Rules_Math::OutputNode& rules,
        const SAO_Definitions_Math::OutputNode& definitions,
        const SAO_Word_Proper_Math::OutputNode& usage,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::OutputNode& characters);

    // Same forward inputs, explicitly chosen output directory (also for tests).
    const OutputNode& BuildAt(const std::filesystem::path& cacheFolder,
        const SAO_English_Rules_Math::OutputNode& rules,
        const SAO_Definitions_Math::OutputNode& definitions,
        const SAO_Word_Proper_Math::OutputNode& usage,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::OutputNode& characters);

    // The previous conductor must materialize and pass its OWN cached bitmap
    // FORWARD. No calls to upstream Orient()/Character() inside this stage.
    // Character +/- changes the exact grouping matched to that input bitmap.
    WordNode Word(const std::vector<CharacterOperation>& operations,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::BitmapCacheNode& inputBitmap,
        Size size, Orientation orientation = Orientation::Left);
    WordNode Word(const std::wstring& text,
        const SAO_UP_LOW_SYM_PUN_NUM_Conductor::BitmapCacheNode& inputBitmap,
        Size size, Orientation orientation = Orientation::Left);

    // Word +/- modifies the ordered sentence grouping. All Plus operations
    // carry previously recached WordNodes FORWARD; no backwards Word() calls.
    // This stage re-caches the sentence and optional composed bitmap.
    // spacePixels is explicit. No guessed font metrics or arbitrary offsets.
    SentenceNode Sentence(const std::vector<WordOperation>& operations,
        Size size, Orientation orientation = Orientation::Left,
        std::uint32_t spacePixels = 0, bool composeBitmap = true);
}

