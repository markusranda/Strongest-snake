#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdint>
#include <vector>
#include <stdexcept>

struct QueueFamily
{
    uint32_t index;
    bool supportsPresent;
};

QueueFamily pickUniversalQueue(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    std::vector<VkQueueFamilyProperties> props(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, props.data());

    for (uint32_t i = 0; i < count; i++)
    {
        if (!(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            continue;

        VkBool32 canPresent = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &canPresent);

        if (canPresent)
            return {i, true};
    }

    throw std::runtime_error("No universal queue family found.");
}
