#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <vulkan/vulkan.h>
#include <memory>
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_renderpass.h"
#include "vulkan_syncobjects.h"
#include "vulkan_commandbuffers.h"

class VulkanSwapchain;

class VulkanDevice;

class VulkanRenderPass;

class VulkanFramebuffers;

class VulkanCommandPool;

class VulkanPipeline;

class VulkanRenderer
{
public:
    VulkanRenderer(HWND hwnd, int width, int height);
    ~VulkanRenderer();

    void init();
    void drawFrame();
    void cleanup();

private:
    HWND hwnd;
    int width;
    int height;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    std::unique_ptr<VulkanDevice> deviceWrapper;
    std::unique_ptr<VulkanSwapchain> swapchain;
    std::unique_ptr<VulkanRenderPass> renderPass;
    std::unique_ptr<VulkanFramebuffers> framebuffers;
    std::unique_ptr<VulkanCommandPool> commandPool;
    std::unique_ptr<VulkanPipeline> pipeline;

    std::unique_ptr<VulkanSyncObjects> syncObjects;
    const int MAX_FRAMES_IN_FLIGHT = 3;
    size_t currentFrame = 0;

    std::unique_ptr<VulkanCommandBuffers> commandBuffers;
};
