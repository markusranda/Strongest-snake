#pragma once
#include <glm/glm.hpp>

struct Quad
{
    glm::vec2 v0, v1, v2, v3; // 4 corners
    glm::vec3 color;

    Quad(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d, glm::vec3 col)
        : v0(a), v1(b), v2(c), v3(d), color(col) {}
};
