#pragma once
#include "Shadertype.h"
#include "RenderLayer.h"
#include "AtlasIndex.h"

struct DrawCmd
{
    uint64_t drawKey;
    ShaderType shaderType;
    RenderLayer renderLayer;
    uint32_t vertexCount;   // usually 6 for quads
    uint32_t firstVertex;   // offset in vertex buffer
    uint32_t instanceCount; // how many instances to draw
    uint16_t z;             // used only for sorting
    uint8_t tiebreak;
    AtlasIndex atlasIndex;
    glm::vec2 atlasOffset;
    glm::vec2 atlasScale;

    DrawCmd() = default;
    DrawCmd(uint64_t drawKey,
            RenderLayer layer,
            ShaderType shader,
            uint16_t z,
            uint8_t tie,
            uint32_t vertexCount,
            uint32_t firstVertex,
            uint32_t instanceCount,
            AtlasIndex atlasIndex,
            glm::vec2 atlasOffset,
            glm::vec2 atlasScale)
        : drawKey(drawKey),
          shaderType(shader),
          renderLayer(layer),
          vertexCount(vertexCount),
          firstVertex(firstVertex),
          instanceCount(instanceCount),
          z(z),
          tiebreak(tie),
          atlasIndex(atlasIndex),
          atlasOffset(atlasOffset),
          atlasScale(atlasScale)
    {
    }
};
