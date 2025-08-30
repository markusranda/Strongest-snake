#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanSwapchain
{
public:
    VulkanSwapchain(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkSurfaceKHR surface,
        int width, int height);
    ~VulkanSwapchain();

    VkSwapchainKHR getSwapchain() const { return swapchain; }
    const std::vector<VkImageView> &getImageViews() const { return imageViews; }
    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat imageFormat;
    VkExtent2D extent;

    // helpers
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &modes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, int width, int height);
};
