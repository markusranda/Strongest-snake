#pragma once
#include <cstdint>

enum class ShaderType : uint16_t
{
    FlatColor,
    SnakeSkin,
    Border,
    Font,
    Texture,
    COUNT
};
