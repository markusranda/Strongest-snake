#pragma once
#include <array>
#include "Atlas.h"

// --------------------------------
// ORES
enum class OreLevel : uint32_t {
    Copper,
    Hematite,
    COUNT
};

struct OrePackage {
    SpriteID spriteID;
    OreLevel level;
};

static constexpr uint32_t ORE_BLOCK_COUNT = 2;
OrePackage ORE_BLOCKS[ORE_BLOCK_COUNT] = {
    { SpriteID::SPR_ORE_BLOCK_COPPER, OreLevel::Copper},
    { SpriteID::SPR_ORE_BLOCK_HEMATITE, OreLevel::Hematite},
};

// --------------------------------
// DRILL
enum class DrillLevel : uint32_t {
    Copper,
    Hematite,
    COUNT
};