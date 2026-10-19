#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SAO_Progression_Conductor.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cwchar>
#include <cstring>
namespace SAO_Progression_Conductor {
    OutputNode Cache{};
    namespace {
        namespace fs = std::filesystem;
        SAO_Progression_Math::BaseStats base;
        fs::path saveFile;
        bool dirty = false;
        constexpr char HEADER[8] = { 'S','A','O','P','R','G','0','1' };
        void Refresh() {
            Cache.stats = SAO_Progression_Math::Build(Cache.state, base);
            const auto& s = Cache.stats;
            std::wostringstream character, harvest;
            character << std::fixed << std::setprecision(3)
                << L"CHARACTER\nLevel: " << s.characterLevel
                << L"\nProgress: " << s.characterPercent << L" / 100%"
                << L"\nDamage: " << s.damage << L"\nClick damage: " << s.clickDamage
                << L"\nStrength: " << base.strength << L"\nDexterity: " << base.dexterity
                << L"\nEndurance: " << base.endurance << L"\nPrecision: " << base.precision;
            harvest << std::fixed << std::setprecision(3)
                << L"HARVESTING / FORESTRY\nLevel: " << s.harvestingLevel
                << L"\nXP: " << double(Cache.state.harvestingXP) / SAO_Progression_Math::UNIT
                << L" / " << double(s.nextHarvestXP) / SAO_Progression_Math::UNIT
                << L"\nYield bonus per bundle: +" << s.harvestingYield
                << L"\n1 XP per full 10-bundle harvest"
                << L"\nEach harvesting level: +1% character progress";
            if (!Cache.ready)harvest << L"\nSelect a player profile to save progress.";
            if (!Cache.error.empty())harvest << L"\n" << Cache.error;
            Cache.characterText = character.str(); Cache.harvestingText = harvest.str(); ++Cache.revision;
        }
        fs::path Saves() {
            wchar_t text[MAX_PATH]{};
            const DWORD n = GetModuleFileNameW(nullptr, text, MAX_PATH);
            if (!n || n >= MAX_PATH)return {};
            auto root = fs::path(std::wstring(text, n)).parent_path();
            for (int i = 0; i < 8 && !root.empty(); ++i) {
                std::error_code ec; auto assets = root / L"SAO_I_E Assets";
                if (fs::is_directory(assets, ec))return assets / L"Player Saves";
                auto parent = root.parent_path(); if (parent == root)break; root = parent;
            }
            return {};
        }
        bool ValidName(const std::wstring& name) {
            if (name.empty() || name.size() > 64 || name == L"." || name == L"..")return false;
            for (auto c : name)if (c < 32 || wcschr(L"\\/:*?\"<>|", c))return false;
            return true;
        }
        bool ReadState(const fs::path& path, SAO_Progression_Math::State& state) {
            std::ifstream in(path, std::ios::binary); char header[8]{};
            if (!in.read(header, 8) || std::memcmp(header, HEADER, 8))return false;
            std::uint64_t* fields[] = { &state.harvestingLevel,&state.harvestingXP,
                &state.subcategoryLevels,&state.grassFraction,&state.strawFraction };
            for (auto* v : fields)if (!in.read(reinterpret_cast<char*>(v), sizeof(*v)))return false;
            // Bound saved counters and validate fractional units before accepting the file.
            return in.peek() == EOF && state.harvestingLevel <= 1000000000ULL &&
                state.subcategoryLevels <= 1000000000ULL && state.subcategoryLevels >= state.harvestingLevel &&
                state.harvestingXP < 20000 + state.harvestingLevel && state.grassFraction < 200 && state.strawFraction < 200;
        }
    }
    bool Save() {
        if (!dirty)return true;
        auto failed = []() {
            if (Cache.error != L"Progress could not be saved; retry pending.") {
                Cache.error = L"Progress could not be saved; retry pending."; Refresh();
            }
            return false;
        };
        if (!Cache.ready || saveFile.empty())return failed();
        const fs::path temporary = saveFile.wstring() + L".tmp";
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out)return failed();
        out.write(HEADER, 8);
        const auto& s = Cache.state;
        const std::uint64_t fields[] = { s.harvestingLevel,s.harvestingXP,s.subcategoryLevels,s.grassFraction,s.strawFraction };
        for (const auto v : fields)out.write(reinterpret_cast<const char*>(&v), sizeof(v));
        out.flush(); if (!out)return failed(); out.close(); if (!out)return failed();
        std::error_code ec;
        if (fs::exists(saveFile, ec)) {
            fs::copy_file(saveFile, fs::path(saveFile.wstring() + L".bak"), fs::copy_options::overwrite_existing, ec);
            if (ec)return failed();
        }
        if (!MoveFileExW(temporary.c_str(), saveFile.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))return failed();
        dirty = false;
        if (!Cache.error.empty()) { Cache.error.clear(); Refresh(); }
        return true;
    }
    bool SetProfile(const std::wstring& name) {
        if (name == Cache.player)return Cache.ready;
        if (!ValidName(name) || !Save())return false;
        const auto folder = Saves(); if (folder.empty())return false;
        std::error_code ec; fs::create_directories(folder, ec); if (ec)return false;
        const auto next = folder / (name + L"_Progress.sao");
        SAO_Progression_Math::State loaded;
        const bool exists = fs::exists(next, ec);
        if (ec || (exists && !ReadState(next, loaded))) {
            Cache.ready = false; Cache.player = name; Cache.state = {};
            Cache.error = L"Progress save could not be loaded; it was not overwritten."; Refresh(); return false;
        }
        saveFile = next; Cache.player = name; Cache.state = loaded; Cache.ready = true; Cache.error.clear(); dirty = false; Refresh(); return true;
    }
    void ReceiveBase(const SAO_Progression_Math::BaseStats& value) {
        if (base.damage == value.damage && base.clickDamage == value.clickDamage &&
            base.strength == value.strength && base.dexterity == value.dexterity &&
            base.endurance == value.endurance && base.precision == value.precision && Cache.revision)return;
        base = value; Refresh();
    }
    Rewards ReceiveHarvest(std::uint32_t grass, std::uint32_t straw, bool completed) {
        if (!Cache.ready)return { grass,straw };
        Rewards result{
            SAO_Progression_Math::HarvestYield(grass,Cache.state.harvestingLevel,Cache.state.grassFraction),
            SAO_Progression_Math::HarvestYield(straw,Cache.state.harvestingLevel,Cache.state.strawFraction) };
        if (completed)SAO_Progression_Math::CompletedHarvest(Cache.state);
        if (grass || straw || completed) { dirty = true; Refresh(); }
        return result;
    }
    void Rollback(const SAO_Progression_Math::State& state) { Cache.state = state; dirty = true; Refresh(); }
}