#pragma once
#include "Shadertype.h"
#include "RenderLayer.h"
#include "Quad.h"

struct DrawCmd
{
    uint32_t vertexCount;
    uint32_t firstVertex;
    RenderLayer renderLayer;
    ShaderType shaderType;
    float z = 0;
    uint8_t tiebreak = 0;
    char *name;

    DrawCmd(RenderLayer renderLayer, ShaderType shaderType, float z, uint8_t tiebreak, uint32_t vertexCount, uint32_t firstVertex, char *name = "not defined")
    {
        this->shaderType = shaderType;
        this->renderLayer = renderLayer;
        this->z = z;
        this->tiebreak = tiebreak;
        this->name = name;
        this->vertexCount = vertexCount;
        this->firstVertex = firstVertex;
    };
};

// 64-bit draw key
// [ 8 bits layer ] [ 16 bits shaderType ] [ 32 bits z-sort value ] [ 8 bits tie-break ]
// | 63 .... 56 | 55 .... 40 | 39 ............ 8 | 7 .... 0 |
// |   Layer    |  Shader    |     Z-Sort       |   Tie    |
inline uint64_t makeDrawKey(RenderLayer layer, ShaderType shader, float z, uint8_t tieBreak)
{
    static_assert(sizeof(RenderLayer) == 1, "RenderLayer must be 8 bits");
    static_assert(sizeof(ShaderType) == 2, "ShaderType must be 16 bits");

    uint64_t key = 0;
    key |= (uint64_t(layer) & 0xFF) << 56;
    key |= (uint64_t(shader) & 0xFFFF) << 40;
    uint32_t zBits = static_cast<uint32_t>((1.0f - z) * 0xFFFFFFFFu);
    key |= (uint64_t)zBits << 8;
    key |= tieBreak;
    return key;
}