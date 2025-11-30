#pragma once
#include "Entity.h"
#include "RenderLayer.h"
#include "Shadertype.h"
#include "AtlasIndex.h"
#include <cstdint>

struct Renderable
{
    Entity entity;
    float z = 0.0f;
    uint8_t tiebreak = 0;
    RenderLayer renderLayer;
    uint64_t drawkey;

    // 64-bit draw key
    // [ 8 bits layer ] [ 16 bits shaderType ] [ 8 bits meshGroup ] [ 8 bits atlasIndex ] [ 16 bits vertexOffset ] [ 8 bits tie ]
    // | 63....56 | 55....40 | 39....32 | 31....24 | 23....8 | 7....0 |
    inline void makeDrawKey(ShaderType shader, AtlasIndex atlasIndex, uint32_t vertexOffset, uint32_t vertexCount)
    {
        static_assert(sizeof(RenderLayer) == 1, "RenderLayer must be 8 bits");
        static_assert(sizeof(ShaderType) == 2, "ShaderType must be 16 bits");

        uint64_t key = 0;
        key |= (uint64_t(renderLayer) & 0xFF) << 56;
        key |= (uint64_t(shader) & 0xFFFF) << 40;
        key |= (uint64_t(atlasIndex) & 0xFF) << 32;
        key |= (uint64_t(vertexOffset & 0xFFFF)) << 16;
        uint16_t zBits = static_cast<uint16_t>((1.0f - z) * 65535.0f);
        key |= (uint64_t)zBits;

        // Commit
        drawkey = key;
    }
};