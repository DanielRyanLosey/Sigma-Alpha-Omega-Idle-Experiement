#include "SAO_Alphabet_Startup.h"
#include "SAO_Alphabet_Conductor.h"
#include "SAO_English_Rules_Math.h"
#include "SAO_Definitions_Math.h"
#include "SAO_Word_Proper_Math.h"
#include "SAO_UP_LOW_SYM_PUN_NUM_Conductor.h"
#include "SAO_Upper_Conductor.h"
#include "SAO_Lower_Conductor.h"
#include "SAO_Symbol_Conductor.h"
#include "SAO_Punctuation_Conductor.h"
#include "SAO_Numbers_Conductor.h"
#include "SAO_Upper_Small_Math.h"
#include "SAO_Upper_Medium_Math.h"
#include "SAO_Upper_Large_Math.h"
#include "SAO_Lower_Small_Math.h"
#include "SAO_Lower_Medium_Math.h"
#include "SAO_Lower_Large_Math.h"
#include "SAO_Symbol_Small_Math.h"
#include "SAO_Symbol_Medium_Math.h"
#include "SAO_Symbol_Large_Math.h"
#include "SAO_Punctuation_Small_Math.h"
#include "SAO_Punctuation_Medium_Math.h"
#include "SAO_Punctuation_Large_Math.h"
#include "SAO_Numbers_Small_Math.h"
#include "SAO_Numbers_Medium_Math.h"
#include "SAO_Numbers_Large_Math.h"
#include <windows.h>
#include <process.h>

namespace SAO_Alphabet_Startup
{
    namespace
    {
        HANDLE worker = nullptr;
        bool Require(bool ready, const wchar_t* stage, const std::wstring& error)
        {
            if (ready) return true;
            SAO_Alphabet_Conductor::SourceFailed(std::wstring(stage) + L": " +
                (error.empty() ? L"upstream cache is not ready" : error));
            return false;
        }

        unsigned __stdcall BuildForward(void*)
        {
            // Each producer builds its cache ONCE, then forwards its result.
            const auto& rules = SAO_English_Rules_Math::Build();
            if (!Require(rules.ready, L"English Rules", rules.error)) return 0;
            const auto& definitions = SAO_Definitions_Math::Build();
            if (!Require(definitions.ready, L"Definitions", definitions.error)) return 0;
            const auto& usage = SAO_Word_Proper_Math::Build();
            if (!Require(usage.ready, L"Word Proper", usage.error)) return 0;

            const auto& upperSmall = SAO_Upper_Small_Math::Build();
            if (!Require(upperSmall.ready, L"Upper Small", upperSmall.error)) return 0;
            const auto& upperMedium = SAO_Upper_Medium_Math::Build();
            if (!Require(upperMedium.ready, L"Upper Medium", upperMedium.error)) return 0;
            const auto& upperLarge = SAO_Upper_Large_Math::Build();
            if (!Require(upperLarge.ready, L"Upper Large", upperLarge.error)) return 0;
            const auto& upper = SAO_Upper_Conductor::Build(upperSmall, upperMedium, upperLarge);
            if (!Require(upper.ready, L"Upper Conductor", upper.error)) return 0;

            const auto& lowerSmall = SAO_Lower_Small_Math::Build();
            if (!Require(lowerSmall.ready, L"Lower Small", lowerSmall.error)) return 0;
            const auto& lowerMedium = SAO_Lower_Medium_Math::Build();
            if (!Require(lowerMedium.ready, L"Lower Medium", lowerMedium.error)) return 0;
            const auto& lowerLarge = SAO_Lower_Large_Math::Build();
            if (!Require(lowerLarge.ready, L"Lower Large", lowerLarge.error)) return 0;
            const auto& lower = SAO_Lower_Conductor::Build(lowerSmall, lowerMedium, lowerLarge);
            if (!Require(lower.ready, L"Lower Conductor", lower.error)) return 0;

            const auto& symbolSmall = SAO_Symbol_Small_Math::Build();
            if (!Require(symbolSmall.ready, L"Symbol Small", symbolSmall.error)) return 0;
            const auto& symbolMedium = SAO_Symbol_Medium_Math::Build();
            if (!Require(symbolMedium.ready, L"Symbol Medium", symbolMedium.error)) return 0;
            const auto& symbolLarge = SAO_Symbol_Large_Math::Build();
            if (!Require(symbolLarge.ready, L"Symbol Large", symbolLarge.error)) return 0;
            const auto& symbols = SAO_Symbol_Conductor::Build(symbolSmall, symbolMedium, symbolLarge);
            if (!Require(symbols.ready, L"Symbol Conductor", symbols.error)) return 0;

            const auto& punctSmall = SAO_Punctuation_Small_Math::Build();
            if (!Require(punctSmall.ready, L"Punctuation Small", punctSmall.error)) return 0;
            const auto& punctMedium = SAO_Punctuation_Medium_Math::Build();
            if (!Require(punctMedium.ready, L"Punctuation Medium", punctMedium.error)) return 0;
            const auto& punctLarge = SAO_Punctuation_Large_Math::Build();
            if (!Require(punctLarge.ready, L"Punctuation Large", punctLarge.error)) return 0;
            const auto& punctuation = SAO_Punctuation_Conductor::Build(punctSmall, punctMedium, punctLarge);
            if (!Require(punctuation.ready, L"Punctuation Conductor", punctuation.error)) return 0;

            const auto& numberSmall = SAO_Numbers_Small_Math::Build();
            if (!Require(numberSmall.ready, L"Number Small", numberSmall.error)) return 0;
            const auto& numberMedium = SAO_Numbers_Medium_Math::Build();
            if (!Require(numberMedium.ready, L"Number Medium", numberMedium.error)) return 0;
            const auto& numberLarge = SAO_Numbers_Large_Math::Build();
            if (!Require(numberLarge.ready, L"Number Large", numberLarge.error)) return 0;
            const auto& numbers = SAO_Numbers_Conductor::Build(numberSmall, numberMedium, numberLarge);
            if (!Require(numbers.ready, L"Numbers Conductor", numbers.error)) return 0;

            const auto& characters = SAO_UP_LOW_SYM_PUN_NUM_Conductor::Build(
                upper, lower, symbols, punctuation, numbers);
            if (!Require(characters.ready, L"Character Conductor", characters.error)) return 0;

            const auto& alphabet = SAO_Alphabet_Conductor::Build(rules, definitions, usage, characters);
            if (!Require(alphabet.ready, L"Alphabet Conductor", alphabet.error)) return 0;
            SAO_Alphabet_Conductor::PrepareVocabulary(); // Forwards answer or failure.
            return 0;
        }
    }
    bool Start()
    {
        if (worker) return true;
        worker = reinterpret_cast<HANDLE>(
            _beginthreadex(nullptr, 0, BuildForward, nullptr, 0, nullptr));
        return worker != nullptr;
    }
    void Stop()
    {
        if (!worker) return;
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
        worker = nullptr;
    }
}
