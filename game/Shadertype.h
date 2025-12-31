#pragma once
#include <cstdint>

enum class ShaderType : uint16_t
{
    FlatColor,
    Border,
    Font,
    Texture,
    TextureScrolling,
    TextureParallax,
    ShadowOverlay,
    UISimpleRect,
    COUNT
};
