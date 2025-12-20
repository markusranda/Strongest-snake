#pragma once
#include "Entity.h"
#include "../RenderLayer.h"
#include "../Shadertype.h"
#include "../AtlasIndex.h"
#include <cstdint>

const int MASK_8_BITS = 0xFF;
const int MASK_16_BITS = 0xFFFF;
const int MASK_16_BITS_F = 65535.0f;

struct DrawKeyParts
{
    RenderLayer layer;
    ShaderType shader;
    uint16_t vertexOffset;
    uint16_t z;
    uint8_t tie;
};

struct Renderable
{
    Entity entity;
    uint16_t z = 0;
    uint8_t tiebreak = 0;
    RenderLayer renderLayer;
    uint64_t drawkey;

    // 64-bit draw key
    // [ 8 bits layer ] [ 16 bits shaderType ] [ 16 bits vertexOffset ] [ 16 bits z ] [ 8 bits tie ]
    // | 63....56       | 55....40             | 39....24               | 23....8     | 7....0 |
    inline void packDrawKey(ShaderType shader, uint32_t vertexOffset)
    {
        static_assert(sizeof(RenderLayer) == 1, "RenderLayer must be 8 bits");
        static_assert(sizeof(ShaderType) == 2, "ShaderType must be 16 bits");

        uint64_t key = 0;
        key |= (uint64_t(renderLayer) & MASK_8_BITS) << 56;
        key |= (uint64_t(shader) & MASK_16_BITS) << 40;
        key |= (uint64_t(vertexOffset) & MASK_16_BITS) << 24;
        key |= (uint64_t(z) & MASK_16_BITS) << 8;
        key |= (uint64_t(tiebreak) & MASK_8_BITS) << 0;

        // Commit
        drawkey = key;
    }
};

inline DrawKeyParts unpackDrawKey(uint64_t key)
{
    DrawKeyParts out{};

    out.layer = static_cast<RenderLayer>((key >> 56) & MASK_8_BITS);
    out.shader = static_cast<ShaderType>((key >> 40) & MASK_16_BITS);
    out.vertexOffset = static_cast<uint16_t>((key >> 24) & MASK_16_BITS);
    out.z = static_cast<uint16_t>((key >> 8) & MASK_16_BITS);
    out.tie = static_cast<uint8_t>(key & MASK_8_BITS);

    return out;
}
