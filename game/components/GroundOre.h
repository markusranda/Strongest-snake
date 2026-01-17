#pragma once
#include "Entity.h"
#include "../Item.h"

struct GroundOre 
{
    ItemId itemId;
    Entity parentRef;
    OreLevel oreLevel;
};