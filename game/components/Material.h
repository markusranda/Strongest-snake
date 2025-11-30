#pragma once
#include "../Shadertype.h"
#include "../AtlasIndex.h"
#include <glm/glm.hpp>

struct Material
{
    glm::vec4 color = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
    ShaderType shaderType;
    AtlasIndex atlasIndex;
    glm::vec2 size;

    Material(ShaderType shaderType, AtlasIndex atlasIndex, glm::vec2 size) : shaderType(shaderType), atlasIndex(atlasIndex), size(size) {}
    Material(glm::vec4 color, ShaderType shaderType, AtlasIndex atlasIndex, glm::vec2 size) : color(color), shaderType(shaderType), atlasIndex(atlasIndex), size(size) {}
};