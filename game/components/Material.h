#pragma once
#include "../Shadertype.h"
#include "../AtlasIndex.h"
#include "../libs/glm/glm.hpp"

struct Material
{
    glm::vec4 color = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
    ShaderType shaderType;
    AtlasIndex atlasIndex;
    glm::vec2 size;
};