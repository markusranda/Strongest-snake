#pragma once
#include "Mesh.h"

namespace MeshRegistry
{
    // --- Meshes ---
    static constexpr Mesh quad = Mesh(0, 6, "Quad");
    static constexpr Mesh triangle = Mesh(6, 3, "Triangle");

    // --- Vertices ---
    // !! Remember to update count if increasing number of vertices !!
    static const std::vector<Vertex> vertices = {
        // Quad
        Vertex{{0.0f, 0.0f}, {0.0f, 0.0f}},
        Vertex{{1.0f, 0.0f}, {1.0f, 0.0f}},
        Vertex{{0.0f, 1.0f}, {0.0f, 1.0f}},
        Vertex{{0.0f, 1.0f}, {0.0f, 1.0f}},
        Vertex{{1.0f, 0.0f}, {1.0f, 0.0f}},
        Vertex{{1.0f, 1.0f}, {1.0f, 1.0f}},
        // Triangle
        Vertex{{0.0f, 0.0f}, {0.0f, 0.0f}},
        Vertex{{1.0f, 0.5f}, {1.0f, 0.5f}},
        Vertex{{0.0f, 1.0f}, {0.0f, 1.0f}},
    };

    static std::vector<Vertex> getVertices(const Mesh &mesh)
    {
        auto it = vertices.begin();
        auto start = it + mesh.vertexOffset;
        auto end = start + mesh.vertexCount;

        assert(start >= vertices.begin() && end <= vertices.end() && "Mesh vertex range out of bounds");

        return std::vector<Vertex>(start, end);
    }
}