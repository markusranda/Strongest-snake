#include "vulkan_framebuffers.h"
#include <stdexcept>

VulkanFramebuffers::VulkanFramebuffers(
    VkDevice device,
    VkRenderPass renderPass,
    const std::vector<VkImageView> &swapchainImageViews,
    VkExtent2D swapchainExtent) : device(device)
{
    framebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++)
    {
        VkImageView attachments[] = {
            swapchainImageViews[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

VulkanFramebuffers::~VulkanFramebuffers()
{
    for (auto fb : framebuffers)
    {
        vkDestroyFramebuffer(device, fb, nullptr);
    }
}
