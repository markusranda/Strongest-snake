#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class VulkanSyncObjects
{
public:
    VulkanSyncObjects(VkDevice device, size_t maxFramesInFlight);
    ~VulkanSyncObjects();

    VkSemaphore &getImageAvailableSemaphore(size_t frameIndex) { return imageAvailableSemaphores[frameIndex]; }
    VkSemaphore &getRenderFinishedSemaphore(size_t frameIndex) { return renderFinishedSemaphores[frameIndex]; }
    VkFence &getInFlightFence(size_t frameIndex) { return inFlightFences[frameIndex]; }

    // const getters too if you need read-only access
    const VkSemaphore &getImageAvailableSemaphore(size_t frameIndex) const { return imageAvailableSemaphores[frameIndex]; }
    const VkSemaphore &getRenderFinishedSemaphore(size_t frameIndex) const { return renderFinishedSemaphores[frameIndex]; }
    const VkFence &getInFlightFence(size_t frameIndex) const { return inFlightFences[frameIndex]; }

private:
    VkDevice device;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
};
