#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Standalone English word -> definition source. No PNG, character, rule,
// display, conductor, or Main Screen dependencies.
namespace SAO_Definitions_Math
{
    struct CacheNode
    {
        std::wstring name;
        std::wstring path;
        std::uint32_t wordCount = 0;
        std::uint32_t definitionCount = 0;
        std::uint64_t contentHash = 0;
        bool ready = false;
    };

    struct OutputNode
    {
        CacheNode cache;
        std::wstring error;
        bool ready = false;
    };

    struct Meaning
    {
        std::uint32_t id = 0;
        std::string partOfSpeech;
        std::string definition;
    };

    struct WordResult
    {
        std::uint32_t id = 0;
        std::string word;
        std::vector<Meaning> meanings;
        bool found = false;
    };

    extern OutputNode Cache;

    // Default input: executable/Assets/Language/SAO_Definitions/*.tsv
    // Default output: executable/Cache/SAO_Definitions_Math/SAO_Definitions.cache
    const OutputNode& Build();

    // Explicit folder paths for deterministic testing or custom source placement.
    // Inputs are TWO distinct files: words.tsv and senses.tsv.
    // Output is a newly cached binary file, reused when source is unchanged.
    const OutputNode& Build(const std::filesystem::path& wordsFile,
        const std::filesystem::path& sensesFile,
        const std::filesystem::path& cacheFolder);

    // Uses a small word/sense index; definition text is read from the cache
    // only when asked for. English ASCII case normalization is explicit.
    WordResult Find(std::string_view word);
}
