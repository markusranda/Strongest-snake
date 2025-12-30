#pragma once
#include <cstdint>

struct Mesh
{
    uint32_t vertexOffset;
    uint32_t vertexCount;
    const char *name;

    constexpr Mesh(uint32_t vOff = 0, uint32_t vCount = 0, const char *name = "unnamed_mesh")
        : vertexOffset(vOff), vertexCount(vCount), name(name) {}

    bool operator==(const Mesh &other) const
    {
        return vertexOffset == other.vertexOffset &&
               vertexCount == other.vertexCount &&
               name == other.name;
    }
};