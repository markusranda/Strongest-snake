#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanCommandPool
{
public:
    VulkanCommandPool(
        VkDevice device,
        uint32_t graphicsQueueFamilyIndex,
        size_t commandBufferCount);
    ~VulkanCommandPool();

    VkCommandPool getCommandPool() const { return commandPool; }
    const std::vector<VkCommandBuffer> &getCommandBuffers() const { return commandBuffers; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
};
