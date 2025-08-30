#include "vulkan_renderer.h"
#include <stdexcept>
#include <iostream>
#include <vulkan/vulkan_win32.h>
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_renderpass.h"
#include "vulkan_framebuffers.h"
#include "vulkan_commandpool.h"
#include "vulkan_pipeline.h"

VulkanRenderer::VulkanRenderer(HWND hwnd, int width, int height)
    : hwnd(hwnd), width(width), height(height) {}

VulkanRenderer::~VulkanRenderer()
{
    cleanup();
}

void VulkanRenderer::init()
{
    // --- Create Vulkan Instance ---
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Strongest Snake";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS)
    {
        std::cerr << "vkCreateInstance failed: " << res << "\n";
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    std::cout << "Vulkan instance created!\n";

    // --- Create Surface (Win32) ---
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hwnd = hwnd;
    surfaceInfo.hinstance = GetModuleHandle(nullptr);

    if (vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Win32 surface");
    }

    std::cout << "Surface created!\n";

    // --- Device wrapper handles physical + logical device ---
    deviceWrapper = std::make_unique<VulkanDevice>(instance, surface);

    // Setup swapchain
    swapchain = std::make_unique<VulkanSwapchain>(
        deviceWrapper->getPhysicalDevice(),
        deviceWrapper->getDevice(),
        surface,
        width, height);

    // Setup render pass
    renderPass = std::make_unique<VulkanRenderPass>(
        deviceWrapper->getDevice(),
        swapchain->getImageFormat());

    // Setup frame buffers
    framebuffers = std::make_unique<VulkanFramebuffers>(
        deviceWrapper->getDevice(),
        renderPass->getRenderPass(),
        swapchain->getImageViews(),
        swapchain->getExtent());

    // Setup command pool
    commandPool = std::make_unique<VulkanCommandPool>(
        deviceWrapper->getDevice(),
        deviceWrapper->getQueueFamilyIndices().graphicsFamily.value(),
        framebuffers->getFramebuffers().size());

    // Setup graphics pipeline
    pipeline = std::make_unique<VulkanPipeline>(
        deviceWrapper->getDevice(),
        swapchain->getExtent(),
        renderPass->getRenderPass(),
        "shaders/triangle.vert.spv",
        "shaders/triangle.frag.spv");

    // Setup synchronization objects
    syncObjects = std::make_unique<VulkanSyncObjects>(
        deviceWrapper->getDevice(),
        MAX_FRAMES_IN_FLIGHT);

    commandBuffers = std::make_unique<VulkanCommandBuffers>(
        deviceWrapper->getDevice(),
        commandPool->getCommandPool(),
        renderPass->getRenderPass(),
        pipeline->getPipeline(),
        framebuffers->getFramebuffers(),
        swapchain->getExtent());
}

void VulkanRenderer::drawFrame()
{
    VkDevice device = deviceWrapper->getDevice();
    VkQueue graphicsQueue = deviceWrapper->getGraphicsQueue();
    VkQueue presentQueue = deviceWrapper->getPresentQueue();

    vkWaitForFences(device, 1, &syncObjects->getInFlightFence(currentFrame), VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &syncObjects->getInFlightFence(currentFrame));

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain->getSwapchain(),
                                            UINT64_MAX,
                                            syncObjects->getImageAvailableSemaphore(currentFrame),
                                            VK_NULL_HANDLE, &imageIndex);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {syncObjects->getImageAvailableSemaphore(currentFrame)};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    VkCommandBuffer cmdBuffer = commandBuffers->getCommandBuffers()[imageIndex];
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    VkSemaphore signalSemaphores[] = {syncObjects->getRenderFinishedSemaphore(currentFrame)};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, syncObjects->getInFlightFence(currentFrame)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain->getSwapchain()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanRenderer::cleanup()
{
    // deviceWrapper destructor automatically cleans up device
    deviceWrapper.reset();

    if (surface)
    {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    if (instance)
    {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }

    swapchain.reset();
    renderPass.reset();
    framebuffers.reset();
    commandPool.reset();
    pipeline.reset();
}
