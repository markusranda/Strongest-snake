#pragma once

#include "Entity.h"

struct Ground
{
    Entity entity;
    float health = 100;
    float maxHealth = 100;
    bool dead = false;
    bool dirty = false;
    Entity treasure = Entity{UINT32_MAX};
};
