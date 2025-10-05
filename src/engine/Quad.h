#pragma once
#include <glm/glm.hpp>
#include "Vertex.h"
#include "ShaderType.h"
#include "RenderLayer.h"

struct Quad
{
    ShaderType shaderType;
    RenderLayer renderLayer;
    glm::vec4 color;
    float z = 0.0f;
    uint8_t tiebreak = 0;
    const char *name = "quad";

    Quad(ShaderType shaderType, RenderLayer renderLayer, glm::vec4 color, const char *name = "quad")
        : shaderType(shaderType), renderLayer(renderLayer), color(color), name(name) {}

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
