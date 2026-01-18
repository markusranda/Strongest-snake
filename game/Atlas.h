#pragma once
#include <glm/glm.hpp>

enum SpriteID : uint32_t
{
    SPR_GROUND_MID_0 = 0,
    SPR_GEM_BLUE = 3,
    SPR_GEM_RED = 4,
    SPR_GEM_GREEN = 5,
    SPR_GEM_ORANGE = 6,
    SPR_GEM_PURPLE = 7,
    SPR_GEMS_BLUE = 8,
    SPR_GEMS_GREEN = 9,
    SPR_GEMS_PURPLE = 10,
    SPR_GEMS_ORANGE = 11,
    
    SPR_GROUND_MID_1 = 128,
    SPR_SKULL = 129,
    
    SPR_GROUND_MID_2 = 256,
    SPR_CAVE_BACKGROUND = 257,
    
    // --- Ground blocks ---
    SPR_GROUND_MID_3 = 384,
    SPR_GROUND_MID_4 = 512,
    SPR_GROUND_MID_5 = 640,
    SPR_GROUND_MID_6 = 768,
    SPR_GROUND_MID_7 = 896,
    SPR_GROUND_MID_8 = 1024,
    SPR_GROUND_MID_9 = 1152,
    SPR_GROUND_MID_10 = 1280,
    SPR_GROUND_MID_11 = 1408,
    SPR_GROUND_MID_12 = 1536,
    SPR_GROUND_MID_13 = 1664,
    SPR_GROUND_MID_14 = 1792,
    SPR_GROUND_MID_15 = 1920,
    SPR_GROUND_MID_16 = 2048,
    SPR_GROUND_MID_17 = 2176,
    SPR_GROUND_MID_18 = 2304,
    SPR_GROUND_MID_19 = 2432,

    // --- Items ---
    SPR_ITM_CPR_DRILL = 1027,    
    SPR_ITM_IRON_DRILL = 1028,
    SPR_ITM_CPR_ENGINE = 517,
    SPR_ITM_IRON_ENGINE = 518,
    SPR_ITM_CPR_LIGHT = 519,
    SPR_ITM_IRON_LIGHT = 520,
    
    SPR_ORE_BLOCK_COPPER = 131,
    SPR_ORE_BLOCK_HEMATITE = 132,

    SPR_ORE_CRUSHED_COPPER = 259,
    SPR_ORE_CRUSHED_IRON = 260,

    SPR_ORE_INGOT_COPPER = 643,
    SPR_ORE_INGOT_IRON = 644,

    // --- Snek ---
    SPR_SNK_SEG_STORAGE = 387,    
    SPR_SNK_SEG_SMELTER = 388,    
    SPR_SNK_SEG_GRINDER = 389,    
    INVALID,
};

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
static constexpr int MAX_ATLAS_ENTRIES = (ATLAS_SIZE.x / ATLAS_CELL_SIZE.x) * (ATLAS_SIZE.y / ATLAS_CELL_SIZE.y);

inline glm::vec4 getUvTransform(AtlasRegion &region)
{
    glm::vec4 model = glm::vec4{
        region.x * ATLAS_CELL_SIZE.x / ATLAS_SIZE.x,
        region.y * ATLAS_CELL_SIZE.y / ATLAS_SIZE.y,
        ATLAS_CELL_SIZE.x / ATLAS_SIZE.x,
        ATLAS_CELL_SIZE.y / ATLAS_SIZE.y,
    };

    return model;
}
