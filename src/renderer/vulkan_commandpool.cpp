#include "vulkan_commandpool.h"
#include <stdexcept>

VulkanCommandPool::VulkanCommandPool(
    VkDevice device,
    uint32_t graphicsQueueFamilyIndex,
    size_t commandBufferCount) : device(device)
{
    // --- Create command pool ---
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsQueueFamilyIndex;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool!");
    }

    // --- Allocate command buffers ---
    commandBuffers.resize(commandBufferCount);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBufferCount);

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

VulkanCommandPool::~VulkanCommandPool()
{
    if (commandPool)
    {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }
}
