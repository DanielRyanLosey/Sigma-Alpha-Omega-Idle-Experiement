#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Standalone deterministic grammatical usage source. No definitions, PNGs,
// screen, conductor, sentence generator, or other Math dependencies.
namespace SAO_Word_Proper_Math
{
    enum class Part : std::uint8_t
    {
        Unknown = 0, Noun, Verb, Adjective, Adverb, Pronoun,
        Preposition, Conjunction, Determiner, Article, Interjection,
        Auxiliary, Modal
    };
    enum class Frame : std::uint8_t
    {
        Unknown = 0, NounPhrase, VerbUnspecified, VerbIntransitive,
        VerbTransitive, VerbDitransitive, VerbFlexible, VerbTo,
        VerbObjectTo, AdjectiveBoth, AdverbModifier, PronounSubject,
        PronounObject, PronounBoth, PronounPossessive,
        PrepositionObject, ToOrPreposition, ConjunctionCoord,
        ConjunctionSub, ConjunctionContext, DeterminerBeforeNoun,
        ArticleBeforeNoun, Interjection, AuxiliaryBe, AuxiliaryHave,
        AuxiliaryDo, ModalBareVerb, Negation
    };
    // Flags are grammatical metadata, NOT numerical word meanings.
    enum Feature : std::uint16_t
    {
        Plural = 1, Singular = 2, MassNoun = 4,
        FirstPerson = 8, SecondPerson = 16,
        NonSingularPerson = 32, ThirdSingularPerson = 64,
        SubjectCase = 128, ObjectCase = 256, PossessiveCase = 512,
        ThirdSingularVerb = 1024, PastVerb = 2048,
        BaseVerb = 4096, ParticipleVerb = 8192
    };
    enum class Result : std::uint8_t { Undetermined, Allowed, Rejected };
    enum class Relation : std::uint8_t
    {
        DeterminerToNoun, AdjectiveToNoun, PrepositionToObject,
        ModalToBaseVerb, InfinitiveToBaseVerb, TransitiveVerbToObject
    };
    enum class VerbForm : std::uint8_t { Base, ThirdPersonSingular, Past, Ing };

    struct Usage
    {
        std::uint32_t senseId = 0; // Same sense ID as SAO_Definitions_Math.
        std::uint16_t wordId = 0;  // Same word ID as SAO_Definitions_Math.
        Part part = Part::Unknown;
        Frame frame = Frame::Unknown;
        std::uint16_t features = 0;
        bool curated = false;    // False means broad class inferred from existing POS.
    };
    struct WordResult
    {
        std::uint32_t wordId = 0;
        std::string word;
        std::vector<Usage> uses;
        bool found = false;
    };
    struct CacheNode
    {
        std::wstring name, path;
        std::uint32_t wordCount = 0, usageCount = 0;
        std::uint32_t bytes = 0;
        std::uint64_t sourceHash = 0;
        bool ready = false;
    };
    struct OutputNode
    {
        CacheNode cache;
        std::wstring error;
        bool ready = false;
    };
    extern OutputNode Cache;

    // Reads this module's own two TSV files; writes only its own .cache.
    const OutputNode& Build();
    const OutputNode& Build(const std::filesystem::path& wordsFile,
        const std::filesystem::path& usageFile,
        const std::filesystem::path& cacheFolder);
    WordResult Find(std::string_view word);
    // A deterministic restricted check, NOT an unrestricted English parser.
    Result CheckPair(std::string_view left, std::string_view right, Relation relation);
    Result CheckArticle(std::string_view article, std::string_view nextWord);
    Result CheckSubjectVerb(std::string_view subject, std::string_view verb);
    std::string Inflect(std::string_view baseVerb, VerbForm form);
}


