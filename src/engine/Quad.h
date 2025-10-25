#pragma once
#include <glm/glm.hpp>
#include "Vertex.h"
#include "ShaderType.h"
#include "RenderLayer.h"

struct Quad
{
    float z = 0.0f;
    uint8_t tiebreak = 0;
    const char *name = "quad";

    Quad(const char *name = "quad") : name(name) {}

    static const std::array<Vertex, 6> &getUnitVertices()
    {
        static const std::array<Vertex, 6> verts = {
            Vertex{{0.0f, 0.0f}, {0.0f, 0.0f}},
            Vertex{{1.0f, 0.0f}, {1.0f, 0.0f}},
            Vertex{{0.0f, 1.0f}, {0.0f, 1.0f}},
            Vertex{{0.0f, 1.0f}, {0.0f, 1.0f}},
            Vertex{{1.0f, 0.0f}, {1.0f, 0.0f}},
            Vertex{{1.0f, 1.0f}, {1.0f, 1.0f}},
        };
        return verts;
    }
};
