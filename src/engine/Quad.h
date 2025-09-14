#pragma once
#include <glm/glm.hpp>

struct Quad
{
    std::array<Vertex, 4> vertices;
    glm::vec3 color;

    Quad::Quad(float x, float y, float w, float h, glm::vec3 c,
               int screenW, int screenH)
        : color(c)
    {
        glm::vec2 topLeft = {x, y};
        glm::vec2 topRight = {x + w, y};
        glm::vec2 bottomLeft = {x, y + h};
        glm::vec2 bottomRight = {x + w, y + h};

        vertices[0] = Vertex{toClipSpace(topLeft, screenW, screenH), color};
        vertices[1] = Vertex{toClipSpace(topRight, screenW, screenH), color};
        vertices[2] = Vertex{toClipSpace(bottomLeft, screenW, screenH), color};
        vertices[3] = Vertex{toClipSpace(bottomRight, screenW, screenH), color};
    }

    static glm::vec2 toClipSpace(glm::vec2 pos, int screenW, int screenH)
    {
        float x = (pos.x / screenW) * 2.0f - 1.0f;
        float y = 1.0f - (pos.y / screenH) * 2.0f;
        return {x, -y};
    }
};
