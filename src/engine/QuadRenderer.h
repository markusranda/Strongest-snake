// QuadRenderer.h
#pragma once
#include <vector>
#include "Quad.h"
#include <vulkan/vulkan.h>

struct QuadRenderer
{
    static std::vector<Vertex> toVertices(const std::vector<Quad> &quads)
    {
        std::vector<Vertex> vertices;
        vertices.reserve(quads.size() * 6); // 2 triangles per quad

        for (const auto &q : quads)
        {
            // expand quad into 6 vertices (triangle list)
            vertices.push_back({q.v0, q.color});
            vertices.push_back({q.v1, q.color});
            vertices.push_back({q.v2, q.color});
            vertices.push_back({q.v2, q.color});
            vertices.push_back({q.v3, q.color});
            vertices.push_back({q.v0, q.color});
        }

        return vertices;
    }
};
