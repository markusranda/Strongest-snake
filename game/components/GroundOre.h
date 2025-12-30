#pragma once
#include "Entity.h"
#include "../Mining.h"
#include "../Item.h"

struct GroundOre {
    ItemId itemId;
    Entity parentRef;
    OreLevel oreLevel;
};