#pragma once
#include <cstdint>

struct Mesh
{
    uint32_t vertexOffset;
    uint32_t vertexCount;
    char *name;

    constexpr Mesh(uint32_t vOff = 0, uint32_t vCount = 0, char *name = "unnamed_mesh") : vertexOffset(vOff), vertexCount(vCount), name(name) {}
};