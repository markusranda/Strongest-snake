#pragma once
#include <glm/glm.hpp>

struct AtlasRegion
{
    uint32_t id;
    char name[32];
    uint16_t x;
    uint16_t y;
    uint8_t width;
    uint8_t height;
    char padding[2];
};

constexpr glm::vec2 ATLAS_CELL_SIZE = glm::vec2(32.0f, 32.0f);
constexpr glm::vec2 FONT_ATLAS_CELL_SIZE = glm::vec2(16.0f, 32.0f);
constexpr glm::vec2 FONT_ATLAS_SIZE = glm::vec2(1456.0f, 32.0f); // Update this when expanding fonts.png
constexpr glm::vec2 ATLAS_SIZE = glm::vec2(4096.0f, 4096.0f);    // Update this when expanding atlas.png

inline glm::vec4 getUvTransform(AtlasRegion &region)
{
    glm::vec4 model = glm::vec4{
        region.x * ATLAS_CELL_SIZE.x / ATLAS_SIZE.x,
        region.y * ATLAS_CELL_SIZE.y / ATLAS_SIZE.y,
        ATLAS_CELL_SIZE.x / ATLAS_SIZE.x,
        ATLAS_CELL_SIZE.y / ATLAS_SIZE.y,
    };

    float epsilonU = 1.0f / ATLAS_SIZE.x;
    float epsilonV = 1.0f / ATLAS_SIZE.y;

    model.x += epsilonU;
    model.y += epsilonV;
    model.z -= 2.0f * epsilonU;
    model.w -= 2.0f * epsilonV;

    return model;
}