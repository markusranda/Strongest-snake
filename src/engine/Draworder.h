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
