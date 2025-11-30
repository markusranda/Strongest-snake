#pragma once
#include "Mesh.h"
#include <vector>

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

    static const Vertex *getVertices(const Mesh &mesh)
    {
        return vertices.data() + mesh.vertexOffset;
    }

    static Vertex getDrillTipLocal()
    {
        return vertices[7]; // Update this if vertices gets a change on i <= 7
    }
}