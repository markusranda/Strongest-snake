#pragma once
#include "Shadertype.h"
#include "RenderLayer.h"
#include "Quad.h"

struct DrawCmd
{
    ShaderType shaderType;
    RenderLayer renderLayer;

    uint32_t vertexCount;   // usually 6 for quads
    uint32_t firstVertex;   // offset in vertex buffer
    uint32_t instanceCount; // how many instances to draw
    uint32_t firstInstance; // instance buffer offset (in instances, not bytes)

    float z; // used only for sorting
    uint8_t tiebreak;

    DrawCmd(RenderLayer layer,
            ShaderType shader,
            float z,
            uint8_t tie,
            uint32_t vertexCount,
            uint32_t firstVertex,
            uint32_t instanceCount,
            uint32_t firstInstance)
        : shaderType(shader),
          renderLayer(layer),
          vertexCount(vertexCount),
          firstVertex(firstVertex),
          instanceCount(instanceCount),
          firstInstance(firstInstance),
          z(z),
          tiebreak(tie)
    {
    }
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