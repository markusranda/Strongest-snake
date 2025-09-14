#include "Mesh.h"
#include "Buffer.h"

Mesh Mesh::create(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    const std::vector<Vertex> &vertices)
{
    Mesh mesh{};
    mesh.vertexCount = static_cast<uint32_t>(vertices.size());
    VkDeviceSize bufferSize = sizeof(Vertex) * mesh.vertexCount;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    CreateBuffer(
        device, physicalDevice, bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory);

    void *data;
    vkMapMemory(device, stagingMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingMemory);

    CreateBuffer(
        device, physicalDevice, bufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        mesh.vertexBuffer, mesh.vertexMemory);

    CopyBuffer(device, commandPool, graphicsQueue, stagingBuffer, mesh.vertexBuffer, bufferSize);

    return mesh;
}

void Mesh::destroy(VkDevice device)
{
    if (vertexBuffer)
    {
        vkDestroyBuffer(device, vertexBuffer, nullptr);
        vertexBuffer = VK_NULL_HANDLE;
    }
    if (vertexMemory)
    {
        vkFreeMemory(device, vertexMemory, nullptr);
        vertexMemory = VK_NULL_HANDLE;
    }
    vertexCount = 0;
}
