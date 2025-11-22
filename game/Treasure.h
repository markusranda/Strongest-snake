#pragma once

#include "Entity.h"

#include <optional>

struct Treasure
{
    std::optional<Entity> groundRef;
};