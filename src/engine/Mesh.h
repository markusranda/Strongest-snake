#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "Vertex.h"
#include "Quad.h"

struct Mesh
{
    VkBuffer vertexBuffer{VK_NULL_HANDLE};
    VkDeviceMemory vertexMemory{VK_NULL_HANDLE};
    uint32_t vertexCount{0};

    // Factory-like function, but private to Engine
    static Mesh create(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        VkCommandPool commandPool,
        VkQueue graphicsQueue,
        const std::array<Vertex, 4> &vertices);

    void destroy(VkDevice device);
};
