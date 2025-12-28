#pragma once
#include "Entity.h"
#include <optional>

struct Ground {
    std::optional<Entity> groundOreRef;
};