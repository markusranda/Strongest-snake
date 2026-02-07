#pragma once
#include "../libs/glm/glm.hpp"

struct CameraPushConstant
{
    alignas(16) glm::mat4 viewProj;
};

static_assert(sizeof(CameraPushConstant) == 64); // If this changes, then you also need to update all offsets in frag shaders

struct FragPushConstant
{
    alignas(8) glm::vec2 cameraWorldPos;
    alignas(4) float globalTime;
    alignas(4) float _pad;
};

static_assert(sizeof(FragPushConstant) == 16);