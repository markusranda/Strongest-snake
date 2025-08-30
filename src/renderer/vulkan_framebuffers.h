#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanFramebuffers
{
public:
    VulkanFramebuffers(
        VkDevice device,
        VkRenderPass renderPass,
        const std::vector<VkImageView> &swapchainImageViews,
        VkExtent2D swapchainExtent);
    ~VulkanFramebuffers();

    const std::vector<VkFramebuffer> &getFramebuffers() const { return framebuffers; }

private:
    VkDevice device = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
};
