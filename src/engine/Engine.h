#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Window.h"
#include "SwapChain.h"
#include "InstanceData.h"
#include "Logger.h"
#include "Vertex.h"
#include "Quad.h"
#include "Pipelines.h"
#include "Draworder.h"
#include "EntityManager.h"
#include "Camera.h"
#include "Transform.h"
#include "Texture.h"
#include "Debug.h"
#include "Pipelines.h"
#include "Draworder.h"

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <optional>
#include <set>
#include <random>
#include <chrono>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

const int MAX_FRAMES_IN_FLIGHT = 3;

class Engine
{
public:
    // Transforms
    std::unordered_map<Entity, Transform> transforms;
    std::unordered_map<Entity, Quad> quads;
    EntityManager ecs;

    Engine(uint32_t width, uint32_t height, Window &window);

    void initVulkan(std::string texturePath);
    void awaitDeviceIdle();
    void cleanup();

    // Draw functions
    void draw(Camera &camera);

private:
    uint32_t width;
    uint32_t height;
    Window window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    QueueFamilyIndices queueFamilies;

    SwapChain swapchain;

    VkRenderPass renderPass;
    std::array<Pipeline, static_cast<size_t>(ShaderType::COUNT)> pipelines;

    // Command
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;

    // Semaphores
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    uint32_t currentFrame = 0;
    std::vector<VkFence> imagesInFlight;

    // Instances
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
    uint32_t maxIntancesPerFrame = 0;

    // Vertices
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t vertexCapacity = 0;
    void *instanceBufferMapped = nullptr;

    // Texture
    VkDescriptorSetLayout textureSetLayout;
    Texture fontTexture;
    VkDescriptorPool descriptorPool;
    VkDescriptorSet descriptorSet;

    void createStaticVertexBuffer();
    uint32_t prepareDraw();
    void endDraw(uint32_t imageIndex);
    void drawCmdList(const std::vector<DrawCmd> &drawCmds, Camera &camera);
    void uploadToInstanceBuffer(const std::vector<InstanceData> &instances);
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createSwapChain();
    void createLogicalDevice();
    void createTexture(std::string texturePath);
    void createDescriptorPool();
    void createDescriptorSet();
    void createRenderPass();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void createImagesInFlight();
    void destroySyncObjects();
    void recreateSwapchain();
    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    bool checkValidationLayerSupport();
    static std::vector<char> readFile(const std::string &filename);
    VkShaderModule createShaderModule(const std::vector<char> &code);
    std::vector<const char *> getRequiredExtensions();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
};