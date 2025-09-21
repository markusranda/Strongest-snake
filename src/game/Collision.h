#pragma once
#include "glm/vec2.hpp"

inline bool rectIntersects(glm::vec2 apos, glm::vec2 asize,
                           glm::vec2 bpos, glm::vec2 bsize)
{
    return (apos.x < bpos.x + bsize.x &&
            apos.x + asize.x > bpos.x &&
            apos.y < bpos.y + bsize.y &&
            apos.y + asize.y > bpos.y);
}