#pragma once
#include "glm/vec2.hpp"
#include "MeshRegistry.h"
#include "components/AABB.h"

// PROFILING
#include "tracy/Tracy.hpp"

inline AABB computeWorldAABB(const Mesh &mesh, const Transform &t)
{
    ZoneScoped;
    glm::vec2 min(FLT_MAX), max(-FLT_MAX);

    const auto &verts = MeshRegistry::getVertices(mesh);
    const glm::mat4 &m = t.model;

    for (size_t i = mesh.vertexOffset; i < mesh.vertexOffset + mesh.vertexCount; i++)
    {
        auto v = verts[i];
        float x = v.pos.x * m[0][0] + v.pos.y * m[1][0] + m[3][0];
        float y = v.pos.x * m[0][1] + v.pos.y * m[1][1] + m[3][1];

        min.x = std::min(min.x, x);
        min.y = std::min(min.y, y);
        max.x = std::max(max.x, x);
        max.y = std::max(max.y, y);
    }

    return {min, max};
}

inline bool rectIntersects(AABB &A, AABB &B)
{
    ZoneScoped;
    return (A.min.x < B.max.x &&
            A.max.x > B.min.x &&
            A.min.y < B.max.y &&
            A.max.y > B.min.y);
}

inline bool rectIntersectsExclusive(AABB &A, AABB &B)
{
    ZoneScoped;
    return (A.min.x < B.max.x &&
            A.max.x > B.min.x &&
            A.min.y < B.max.y &&
            A.max.y > B.min.y);
}

inline bool rectFullyInside(AABB &A, AABB &B)
{
    ZoneScoped;
    return (A.min.x >= B.min.x &&
            A.max.x <= B.max.x &&
            A.min.y >= B.min.y &&
            A.max.y <= B.max.y);
}

inline bool circleIntersectsAABB(glm::vec2 center, float radius, AABB box)
{
    float cx = glm::clamp(center.x, box.min.x, box.max.x);
    float cy = glm::clamp(center.y, box.min.y, box.max.y);
    float dx = center.x - cx;
    float dy = center.y - cy;
    return (dx * dx + dy * dy) <= radius * radius;
}
