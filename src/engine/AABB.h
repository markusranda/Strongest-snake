#pragma once
#include "glm/vec2.hpp"

struct AABB
{
    glm::vec2 min; // bottom-left corner
    glm::vec2 max; // top-right corner
};
