#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanCommandBuffers
{
public:
    VulkanCommandBuffers(VkDevice device, VkCommandPool commandPool,
                         VkRenderPass renderPass, VkPipeline graphicsPipeline,
                         const std::vector<VkFramebuffer> &framebuffers,
                         VkExtent2D extent);
    ~VulkanCommandBuffers();

    const std::vector<VkCommandBuffer> &getCommandBuffers() const { return commandBuffers; }

private:
    VkDevice device;
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
};
