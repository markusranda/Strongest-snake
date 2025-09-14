#pragma once
#include <glm/glm.hpp>

struct Colors
{
    static constexpr uint32_t WHITE = 0xFFFFFF;
    static constexpr uint32_t SKY_BLUE = 0xA6D9F7;
    static constexpr uint32_t SAND_BEIGE = 0xF8E1A1;
    static constexpr uint32_t GROUND_BEIGE = 0x89724a;
    static constexpr uint32_t LIME_GREEN = 0xB9E769;
    static constexpr uint32_t LEAF_GREEN = 0x6FBF73;
    static constexpr uint32_t MANGO_ORANGE = 0xFFB347;
    static constexpr uint32_t STRAWBERRY_RED = 0xF76C6C;
    static constexpr uint32_t OCEAN_TEAL = 0x5EDFD4;
    static constexpr uint32_t SUN_YELLOW = 0xFFF178;

    static glm::vec4 fromHex(uint32_t hex, float alpha)
    {
        float r = ((hex >> 16) & 0xFF) / 255.0f;
        float g = ((hex >> 8) & 0xFF) / 255.0f;
        float b = (hex & 0xFF) / 255.0f;
        return {r, g, b, alpha};
    };
};
