#pragma once
#include <glm/glm.hpp>
#include "Pipelines.h"
#include "Shadertype.h"
#include "RenderLayer.h"

struct Quad
{
    std::array<Vertex, 6> vertices;
    glm::vec4 color;
    ShaderType shaderType;
    RenderLayer renderLayer;
    float z = 0;
    uint8_t tiebreak = 0;
    char *name;

    Quad::Quad(float x, float y, float w, float h,
               glm::vec4 c, ShaderType shaderType,
               RenderLayer renderLayer, char *name = "not defined")
        : color(c), shaderType(shaderType), name(name), renderLayer(renderLayer)
    {
        glm::vec2 topLeft = {x, y};
        glm::vec2 topRight = {x + w, y};
        glm::vec2 bottomLeft = {x, y + h};
        glm::vec2 bottomRight = {x + w, y + h};

        vertices[0] = Vertex{topLeft, color, glm::vec2{0, 0}};
        vertices[1] = Vertex{topRight, color, glm::vec2{1, 0}};
        vertices[2] = Vertex{bottomLeft, color, glm::vec2{0, 1}};
        vertices[3] = Vertex{bottomLeft, color, glm::vec2{0, 1}};
        vertices[4] = Vertex{topRight, color, glm::vec2{1, 0}};
        vertices[5] = Vertex{bottomRight, color, glm::vec2{1, 1}};
    }
};
