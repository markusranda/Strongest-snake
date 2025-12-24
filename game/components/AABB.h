#pragma once
#include "Logrador.h"
#include "Entity.h"

#include "glm/vec2.hpp"

const uint32_t MAX_AABB_NODES = 4;

// ---- Forward declarations ----
struct AABB;
struct AABBNode;
struct AABBNodePool;
struct AABBNodePoolBoy;

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

    glm::vec2 center()
    {
        return max - min;
    }
};
