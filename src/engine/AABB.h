#pragma once
#include "glm/vec2.hpp"

struct AABB
{
    glm::vec2 min; // bottom-left corner
    glm::vec2 max; // top-right corner

    glm::vec2 size() const { return max - min; }

    void moveTo(const glm::vec2 &pos)
    {
        glm::vec2 halfSize = size() * 0.5f;
        min = pos - halfSize;
        max = pos + halfSize;
    }
};
