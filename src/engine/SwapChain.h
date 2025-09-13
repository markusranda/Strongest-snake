#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "Window.h"

struct SwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class SwapChain
{
public:
    SwapChain() = default;
    ~SwapChain();

    void create(VkPhysicalDevice physicalDevice,
                VkDevice device,
                VkSurfaceKHR surface,
                Window *window,
                uint32_t graphicsFamily,
                uint32_t presentFamily);

    void cleanup(VkDevice device);

    void recreate(VkPhysicalDevice physicalDevice,
                  VkDevice device,
                  VkSurfaceKHR surface,
                  Window *window,
                  uint32_t graphicsFamily,
                  uint32_t presentFamily);

    void createFramebuffers(VkDevice device, VkRenderPass renderPass);

    // Getters
    VkFormat getImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getExtent() const { return swapChainExtent; }
    const std::vector<VkFramebuffer> &getFramebuffers() const { return swapChainFramebuffers; }
    VkSwapchainKHR getHandle() const { return swapChain; }
    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    VkSwapchainKHR swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    void createSwapChain(VkPhysicalDevice physicalDevice,
                         VkDevice device,
                         VkSurfaceKHR surface,
                         Window *window,
                         uint32_t graphicsFamily,
                         uint32_t presentFamily);

    void createImageViews(VkDevice device);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, Window &window);
};
