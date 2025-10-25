#pragma once
#include "EntityManager.h"

namespace MeshRegistry
{
    // --- Meshes ---
    static constexpr Mesh quad = Mesh(0, 6, "Quad");
    static constexpr Mesh triangle = Mesh(6, 3, "Triangle");

    // --- Vertices ---
    static const std::array<Vertex, 6> &quadVertices()
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
    static const std::array<Vertex, 3> &triangleVertices()
    {
        static const std::array<Vertex, 3> verts = {
            Vertex{{0.0f, 0.0f}, {0.0f, 0.0f}},
            Vertex{{1.0f, 0.5f}, {1.0f, 0.5f}},
            Vertex{{0.0f, 1.0f}, {0.0f, 1.0f}},
        };
        return verts;
    }
}