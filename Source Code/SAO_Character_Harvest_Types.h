#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <string>

namespace SAO_Character_Harvest
{
    constexpr UINT COUNT = 23;
    constexpr UINT WIDTH = 1920;
    constexpr UINT HEIGHT = 1080;
    constexpr UINT STRIDE = WIDTH * 4;
    constexpr UINT BYTES = STRIDE * HEIGHT;
    constexpr UINT PARTS = 11;

    enum class Part : UINT
    {
        Feet, Head, Legs, LFA, LH, LUA, RFA, RH, RUA, Tool, Torso
    };
    enum class Polarity : std::int8_t { Plus = 1, Minus = -1 };

    struct BitmapNode
    {
        std::wstring name, path;
        bool ready = false;
    };
    struct StatNode
    {
        double damage = 1.0;
        double strength = 1.0;
        double dexterity = 1.0;
        double endurance = 1.0;
        double precision = 1.0;
    };
    struct MathNode
    {
        std::array<BitmapNode, COUNT> frames{};
        std::wstring error;
        bool ready = false;
        bool complete = false;
    };
    struct PieceStep
    {
        const BitmapNode* frame = nullptr;
        Polarity sign = Polarity::Plus;
        UINT next = 0;
    };
    struct PieceNode
    {
        Part part = Part::Feet;
        std::array<PieceStep, COUNT> steps{};
        StatNode stats{};
        std::wstring error;
        bool ready = false;
        bool complete = false;
    };
}
