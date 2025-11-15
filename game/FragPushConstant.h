#pragma once
#include "glm/glm.hpp"

struct FragPushConstant
{
    alignas(8) glm::vec2 atlasOffset;
    alignas(8) glm::vec2 atlasScale;
    alignas(4) float globalTime;
    float _pad[6];
};

static_assert(sizeof(FragPushConstant) == 48, "FragPushConstant must be 48 bytes");