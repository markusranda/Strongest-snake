#pragma once
#include "../Logrador.h"
#include "Entity.h"

#include "../../libs/glm/glm.hpp"

const uint32_t MAX_AABB_NODES = 4;

// ---- Forward declarations ----
struct AABB;
struct AABBNode;
struct AABBNodePool;
struct AABBNodePoolBoy;

struct AABB
{
    glm::vec2 min; 
    glm::vec2 max; 

    glm::vec2 size() const { return max - min; }
};
