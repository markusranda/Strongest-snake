#pragma once
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "../Camera.h"

#ifdef _DEBUG
#include <tracy/TracyVulkan.hpp>
#endif

struct FrameCtx
{
    VkCommandBuffer cmd;
    Camera camera;
    glm::vec2 extent;
    uint32_t imageIndex;
    float delta;

#ifdef _DEBUG
    // GPU profiling
    TracyVkCtx tracyVkCtx;
#endif
};
