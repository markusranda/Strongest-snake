#pragma once
#include <vulkan/vulkan.h>

// Creates a buffer and allocates memory for it
void CreateBuffer(VkDevice device,
                  VkPhysicalDevice physicalDevice,
                  VkDeviceSize size,
                  VkBufferUsageFlags usage,
                  VkMemoryPropertyFlags properties,
                  VkBuffer &buffer,
                  VkDeviceMemory &bufferMemory);

// Copy contents from one buffer to another
void CopyBuffer(VkDevice device,
                VkCommandPool commandPool,
                VkQueue graphicsQueue,
                VkBuffer srcBuffer,
                VkBuffer dstBuffer,
                VkDeviceSize size);

// Find a suitable memory type index
uint32_t FindMemoryType(VkPhysicalDevice physicalDevice,
                        uint32_t typeFilter,
                        VkMemoryPropertyFlags properties);
