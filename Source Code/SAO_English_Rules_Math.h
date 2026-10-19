#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace SAO_English_Rules_Math
{
    constexpr std::size_t RULE_COUNT = 166;
    constexpr std::size_t CATEGORY_COUNT = 12;
    constexpr std::uint16_t CACHE_VERSION = 1;

    enum class Category : std::uint8_t
    {
        Sentence = 1,
        Capital = 2,
        Comma = 3,
        Punctuation = 4,
        Quote = 5,
        Apostrophe = 6,
        Spacing = 7,
        Noun = 8,
        Verb = 9,
        Pronoun = 10,
        Syntax = 11,
        Style = 12,
    };

    enum class Mode : std::uint8_t
    {
        Foundation = 1,      // Usually firm written-English rule.
        Context = 2,         // Needs a syntactic/semantic decision from later stages.
        Style = 3            // Selectable editorial convention, not a universal rule.
    };

    enum class Action : std::uint8_t
    {
        Recognize = 1,
        Separate = 2,
        Order = 3,
        Preserve = 4,
        Capitalize = 5,
        Lowercase = 6,
        Insert = 7,
        Omit = 8,
        Select = 9,
        Resolve = 10,
        Punctuate = 11,
        Space = 12,
        Agree = 13,
    };

    // Exactly eight bytes. One authoritative descriptor per rule.
    // No definitions, copied text, glyph images or retained input strings.
    struct RuleNode
    {
        std::uint16_t id = 0;
        Category category = Category::Sentence;
        Mode mode = Mode::Context;
        Action action = Action::Recognize;
        std::uint8_t priority = 0;
        std::uint8_t flags = 0;
        std::uint8_t reserved = 0;
    };
    static_assert(sizeof(RuleNode) == 8, "SAO RuleNode must remain eight bytes");

    struct GroupNode
    {
        Category category = Category::Sentence;
        std::uint16_t firstId = 0;
        std::uint16_t count = 0;
    };

    struct RuleCacheNode
    {
        std::wstring name;
        std::wstring path;
        std::uint32_t bytes = 0;
        std::uint16_t ruleCount = 0;
        bool ready = false;
    };

    struct OutputNode
    {
        RuleCacheNode rules;
        std::array<GroupNode, CATEGORY_COUNT> groups{};
        std::wstring error;
        bool ready = false;
    };

    extern OutputNode Cache;

    // Own rule table -> tiny independent .cache -> OutputNode.
    // Does not call any other math or conductor.
    const OutputNode& Build();

    // For a caller that already knows the folder (useful for controlled tests).
    // Creates only this module's rule cache there; no duplicate per sentence.
    const OutputNode& BuildAt(const std::wstring& folder);

    // Lookup never loads a dictionary or parses language. Rules are fixed IDs.
    const RuleNode* Find(std::uint16_t id);
    GroupNode Group(Category category);
}
