#pragma once
#include <vulkan/vulkan.h>

class VulkanRenderPass
{
public:
    VulkanRenderPass(VkDevice device, VkFormat swapchainImageFormat);
    ~VulkanRenderPass();

    VkRenderPass getRenderPass() const { return renderPass; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
};
