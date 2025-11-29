#pragma once

#include "Window.h"
#include "SwapChain.h"
#include "InstanceData.h"
#include "Logrador.h"
#include "Vertex.h"
#include "MeshRegistry.h"
#include "Pipelines.h"
#include "DrawCmd.h"
#include "EntityManager.h"
#include "Camera.h"
#include "Transform.h"
#include "Texture.h"
#include "Debug.h"
#include "Pipelines.h"
#include "Atlas.h"
#include "PushConstants.h"
#include "ParticleSystem.h"
#include "SnakeMath.h"
#include "Material.h"
#include "Renderable.h"
#include "Text.h"
#include "QueueFamily.h"
#include "RendererBarriers.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
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

// PROFILING
#include "tracy/Tracy.hpp"

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};

const int MAX_FRAMES_IN_FLIGHT = 3;

struct Engine
{
    Engine(uint32_t width, uint32_t height, Window &window) : width(width), height(height), window(window) {}

    EntityManager ecs;
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

    QueueFamily queuefamily;

    SwapChain swapchain;
    std::vector<VkImageLayout> swapchainImageLayouts;

    std::array<Pipeline, static_cast<size_t>(ShaderType::COUNT)> pipelines;

    // Particle system
    ParticleSystem particleSystem;

    // MSAA
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

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
    std::vector<InstanceData> instances;
    std::vector<DrawCmd> drawCmds;

    // Vertices
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t vertexCapacity = 0;
    void *instanceBufferMapped = nullptr;

    // Texture
    VkDescriptorSetLayout textureSetLayout;
    Texture fontTexture;
    Texture atlasTexture;
    AtlasRegion *atlasRegions = new AtlasRegion[MAX_ATLAS_ENTRIES];
    VkDescriptorPool descriptorPool;
    std::array<VkDescriptorSet, 2> descriptorSets;

    float globalTime = 0.0f;

    void init()
    {
        try
        {
            Logrador::info("Engine is being created");
            createInstance();
            createDebugMessenger();
            createSurface();
            pickPhysicalDevice();
            pickMsaaSampleCount();
            createQueueFamily();
            createLogicalDevice();
            createCommandPool();
            createTextures();
            createSwapChain();
            createColorResources();
            createGraphicsPipeline();
            createAtlasData();
            createDescriptorPool();
            createDescriptorSets();
            createStaticVertexBuffer();
            createImagesInFlight();
            createCommandBuffers();
            createSyncObjects();
            createParticleSystem();
            Logrador::info("Engine is complete");
        }
        catch (const std::exception &e)
        {
            throw std::runtime_error(std::string("Failed to construct engine: ") + e.what());
        }
        catch (...)
        {
            throw std::runtime_error(std::string("Unknown exception thrown in init: "));
        }
    }

    void awaitDeviceIdle()
    {
        ZoneScopedN("Awaiting device idle");
        vkDeviceWaitIdle(device);
    }

    void createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport())
        {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Strongest snake";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "<insert-engine-name-here>";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_4;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            PopulateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;

            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create instance!");
        }
    }

    void createDebugMessenger()
    {
        if (!enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        PopulateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void createSurface()
    {
        auto status = glfwCreateWindowSurface(instance, window.handle, nullptr, &surface);
        if (status != VK_SUCCESS)
        {
            char buf[128]; // plenty of space for your message
            snprintf(buf, sizeof(buf), "failed to create window surface: VkResult %d", status);
            std::string msg(buf);
            throw std::runtime_error(msg);
        }
    }

    void pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

        if (deviceCount == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto &device : devices)
        {
            if (isDeviceSuitable(device, surface))
            {
                physicalDevice = device;
                break;
            }
        }

        if (physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    void createSwapChain()
    {
        swapchain.create(
            physicalDevice,
            device,
            surface,
            &window);
        swapchainImageLayouts.assign(swapchain.swapChainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    }

    void createColorResources()
    {
        VkFormat colorFormat = swapchain.swapChainImageFormat;

        CreateImage(
            device,
            physicalDevice,
            swapchain.swapChainExtent.width,
            swapchain.swapChainExtent.height,
            msaaSamples,
            colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            colorImage,
            colorImageMemory);

        colorImageView = CreateImageView(device, colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
    }

    void createQueueFamily()
    {
        queuefamily = pickUniversalQueue(physicalDevice, surface);
    }

    void createLogicalDevice()
    {
        uint32_t idx = queuefamily.index;
        float priority = 1.0f;
        VkDeviceQueueCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        info.queueFamilyIndex = idx;
        info.queueCount = 1;
        info.pQueuePriorities = &priority;

        VkPhysicalDeviceDynamicRenderingFeaturesKHR dyn{};
        dyn.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dyn.dynamicRendering = VK_TRUE;
        dyn.pNext = nullptr;

        VkPhysicalDeviceSynchronization2Features sync2{};
        sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
        sync2.synchronization2 = VK_TRUE;
        sync2.pNext = &dyn;

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &info;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.pNext = &sync2;

        if (enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(device, queuefamily.index, 0, &graphicsQueue);
        presentQueue = graphicsQueue;
    }

    void createTextures()
    {
        atlasTexture = LoadTexture("assets/atlas.png",
                                   device,
                                   physicalDevice,
                                   commandPool,
                                   graphicsQueue);

        fontTexture = LoadTexture("assets/fonts.png",
                                  device,
                                  physicalDevice,
                                  commandPool,
                                  graphicsQueue);
    }

    void createAtlasData()
    {
        std::ifstream in("assets/atlas.rigdb", std::ios::binary);
        if (!in)
        {
            throw std::runtime_error("Failed to open file");
        }
        char buffer[sizeof(AtlasRegion)];
        while (in.read(buffer, sizeof(buffer)))
        {
            AtlasRegion region = reinterpret_cast<AtlasRegion &>(buffer);
            atlasRegions[region.id] = region;
        }
    }

    void createDescriptorPool()
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 2;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 2;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

    void createDescriptorSets()
    {
        std::array<VkDescriptorSetLayout, 2> layouts = {textureSetLayout, textureSetLayout};
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
        allocInfo.pSetLayouts = layouts.data();

        if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor set!");

        VkDescriptorImageInfo fontInfo{};
        fontInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        fontInfo.imageView = fontTexture.view;
        fontInfo.sampler = fontTexture.sampler;

        VkDescriptorImageInfo atlasInfo{};
        atlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        atlasInfo.imageView = atlasTexture.view;
        atlasInfo.sampler = atlasTexture.sampler;

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets[(size_t)AtlasIndex::Sprite];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &atlasInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets[(size_t)AtlasIndex::Font];
        descriptorWrites[1].dstBinding = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &fontInfo;

        vkUpdateDescriptorSets(device,
                               static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(),
                               0, nullptr);
    }

    void createGraphicsPipeline()
    {
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &samplerLayoutBinding;

        vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &textureSetLayout);

        pipelines = CreateGraphicsPipelines(device, textureSetLayout, swapchain, msaaSamples);
    }

    void createImagesInFlight()
    {
        imagesInFlight.assign(swapchain.swapChainImages.size(), VK_NULL_HANDLE);
    }

    void createCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queuefamily.index;

        if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create graphics command pool!");
        }
    }

    void createCommandBuffers()
    {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createSyncObjects()
    {
        size_t imageCount = swapchain.swapChainImages.size();

        imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
        renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        // Create per-frame semaphores + fences
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS ||
                vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create per-frame sync objects!");
            }
        }
    }

    void createParticleSystem()
    {
        particleSystem.init(device, physicalDevice, msaaSamples, swapchain);
    }

    void pickMsaaSampleCount()
    {
        VkPhysicalDeviceProperties physicalDeviceProperties;
        vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

        VkSampleCountFlags counts =
            physicalDeviceProperties.limits.framebufferColorSampleCounts &
            physicalDeviceProperties.limits.framebufferDepthSampleCounts;

        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        if (counts & VK_SAMPLE_COUNT_8_BIT)
            samples = VK_SAMPLE_COUNT_8_BIT;
        else if (counts & VK_SAMPLE_COUNT_4_BIT)
            samples = VK_SAMPLE_COUNT_4_BIT;
        else if (counts & VK_SAMPLE_COUNT_2_BIT)
            samples = VK_SAMPLE_COUNT_2_BIT;

        msaaSamples = samples;
    }

    void destroySyncObjects()
    {
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (imageAvailableSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
                imageAvailableSemaphores[i] = VK_NULL_HANDLE;
            }

            if (renderFinishedSemaphores[i] != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
                renderFinishedSemaphores[i] = VK_NULL_HANDLE;
            }

            if (inFlightFences[i] != VK_NULL_HANDLE)
            {
                vkDestroyFence(device, inFlightFences[i], nullptr);
                inFlightFences[i] = VK_NULL_HANDLE;
            }
        }
    }

    bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
    {
        if (!checkDeviceExtensionSupport(device))
            return false;

        auto swapChainSupport = swapchain.querySwapChainSupport(device, surface);
        bool swapChainAdequate = !swapChainSupport.formats.empty() &&
                                 !swapChainSupport.presentModes.empty();

        return swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const auto &extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    std::vector<const char *> getRequiredExtensions()
    {
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport()
    {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char *layerName : validationLayers)
        {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

    void buildInstanceData()
    {
        ZoneScoped; // PROFILER

        ShaderType currentShader = ShaderType::COUNT;
        RenderLayer currentLayer = RenderLayer::World;
        uint32_t currentVertexCount = UINT32_MAX;
        uint32_t currentVertexOffset = UINT32_MAX;
        float currentZ = 0.0f;
        uint8_t currentTiebreak = 0;
        uint32_t instanceOffset = 0;
        AtlasIndex currentAtlasIndex = AtlasIndex::Sprite;
        glm::vec2 currentAtlasOffset;
        glm::vec2 currentAtlasScale;

        instances.clear();
        {
            ZoneScopedN("Sorting renderables");
            std::sort(
                ecs.activeEntities.begin(),
                ecs.activeEntities.end(),
                [this](Entity &a, Entity &b)
                {
                    Renderable &rA = ecs.renderables[ecs.entityToRenderable[entityIndex(a)]];
                    Renderable &rB = ecs.renderables[ecs.entityToRenderable[entityIndex(b)]];
                    return rA.drawkey < rB.drawkey;
                });
        }

        if (ecs.activeEntities.empty())
            return;

        Renderable &firstRenderable = ecs.renderables[ecs.entityToRenderable[entityIndex(ecs.activeEntities[0])]];
        uint64_t prevDrawKey = firstRenderable.drawkey;
        for (Entity entity : ecs.activeEntities)
        {
            uint32_t entityId = entityIndex(entity);
            uint32_t &rId = ecs.entityToRenderable[entityId];
            uint32_t &mId = ecs.entityToMesh[entityId];
            uint32_t &tId = ecs.entityToTransform[entityId];
            uint32_t &matId = ecs.entityToMaterial[entityId];
            uint32_t &uvId = ecs.entityToUvTransforms[entityId];

            Renderable &renderable = ecs.renderables[rId];
            Mesh &mesh = ecs.meshes[mId];
            Transform &transform = ecs.transforms[tId];
            Material &material = ecs.materials[matId];
            glm::vec4 &uvTransform = ecs.uvTransforms[uvId];
            glm::vec2 &atlasOffset = glm::vec2{uvTransform.x, uvTransform.y};
            glm::vec2 &atlasScale = glm::vec2{uvTransform.z, uvTransform.w};

            if (renderable.drawkey != prevDrawKey && !instances.empty())
            {
                uint32_t instanceCount = instances.size() - instanceOffset;
                drawCmds.emplace_back(currentLayer,
                                      currentShader,
                                      currentZ,
                                      currentTiebreak,
                                      currentVertexCount,
                                      currentVertexOffset,
                                      instanceCount,
                                      instanceOffset,
                                      material.atlasIndex,
                                      atlasOffset,
                                      atlasScale);
                instanceOffset = instances.size();
            }

            InstanceData instance = {transform.model, material.color, uvTransform, transform.size, material.size};
            instances.push_back(instance);

            currentShader = material.shaderType;
            currentLayer = renderable.renderLayer;
            currentZ = renderable.z;
            currentTiebreak = renderable.tiebreak;
            currentVertexCount = mesh.vertexCount;
            currentVertexOffset = mesh.vertexOffset;
            currentAtlasIndex = material.atlasIndex;
            currentAtlasOffset = atlasOffset;
            currentAtlasScale = atlasScale;

            // Save current drawKey for next comparison
            prevDrawKey = renderable.drawkey;
        }

        // Last batch
        if (!instances.empty())
        {
            uint32_t instanceCount = instances.size() - instanceOffset;
            drawCmds.emplace_back(currentLayer,
                                  currentShader,
                                  currentZ,
                                  currentTiebreak,
                                  currentVertexCount,
                                  currentVertexOffset,
                                  instanceCount,
                                  instanceOffset,
                                  currentAtlasIndex,
                                  currentAtlasOffset,
                                  currentAtlasScale);
        }
    }

    void buildTextInstances(float fps, glm::vec2 &playerCoords)
    {
        ZoneScoped;

        {
            char text[32];
            std::snprintf(text, sizeof(text), "FPS: %d", (int)fps);
            uint32_t offset = instances.size();
            createTextInstances(text, sizeof(text), glm::vec2(-1.0f, -1.0f), instances);
            uint32_t count = instances.size() - offset;

            DrawCmd dc(
                RenderLayer::World, // Add new layer for UI
                ShaderType::Font,
                0.0f, 0,
                6, 0,
                count,
                offset,
                AtlasIndex::Font,
                glm::vec2{},
                glm::vec2{});
            drawCmds.emplace_back(dc);
        }

        {
            char text[32];
            std::snprintf(text, sizeof(text), "POS: (%d, %d)", (int)playerCoords.x, (int)playerCoords.y);
            uint32_t offset = instances.size();
            createTextInstances(text, sizeof(text), glm::vec2(-1.0f, 0.9f), instances);
            uint32_t count = instances.size() - offset;

            DrawCmd dc(
                RenderLayer::World, // Add new layer for UI
                ShaderType::Font,
                0.0f, 0,
                6, 0,
                count,
                offset,
                AtlasIndex::Font,
                glm::vec2{},
                glm::vec2{});
            drawCmds.emplace_back(dc);
        }
    }

    void buildDebugChunkInstances()
    {
        ZoneScoped;

        uint32_t offset = instances.size();
        ecs.collectChunkDebugInstances(instances);
        uint32_t count = instances.size() - offset;

        DrawCmd dc(
            RenderLayer::World,
            ShaderType::Border, // or a basic flat color shader
            0.0f, 0,
            6, 0,
            count,
            offset,
            AtlasIndex::Sprite, // or whatever fits your pipeline
            glm::vec2{},
            glm::vec2{});
        drawCmds.emplace_back(dc);
    }

    void draw(Camera &camera, float fps, glm::vec2 playerCoords, float delta)
    {
        ZoneScoped; // PROFILER

        int32_t imageIndex = prepareDraw(delta);
        if (imageIndex < 0)
        {
            Logrador::debug("Skipping draw : Recreating swapchain");
            return;
        }

        // Collect all instance data
        drawCmds.clear();
        buildInstanceData();
        buildTextInstances(fps, playerCoords);
        buildDebugChunkInstances();

        for (auto &cmd : drawCmds)
            assert(cmd.firstInstance + cmd.instanceCount <= instances.size());

        //  Upload once, then draw all
        uploadToInstanceBuffer();
        drawCmdList(camera);
        endDraw(imageIndex);
        // particleSystem.resetSpawn();

        FrameMark;
    }

    int32_t prepareDraw(float delta)
    {
        ZoneScoped; // PROFILER
        waitForFence(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result = acquireNextImageKHR(
            device,
            swapchain.handle,
            UINT64_MAX,
            imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE,
            &imageIndex);

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE)
        {
            waitForFence(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }

        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapchain();
            return -1;
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        VkCommandBuffer commandBuffer = commandBuffers[currentFrame];
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin command buffer");
        }

        barrierPresentToColor(swapchain, swapchainImageLayouts, imageIndex, commandBuffer);

        // Dynamic rendering setup (MSAA color + resolve to swapchain)
        VkRenderingAttachmentInfoKHR colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        colorAttachment.imageView = colorImageView; // MSAA image
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        colorAttachment.resolveImageView = swapchain.swapChainImageViews[imageIndex];
        colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {0.5f, 0.5f, 0.5f, 1.0f};

        VkRenderingInfoKHR renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        renderInfo.renderArea.offset = {0, 0};
        renderInfo.renderArea.extent = swapchain.swapChainExtent;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachment;

        // Compute pass before rendering
        particleSystem.recordCompute(commandBuffer, delta);

        vkCmdBeginRendering(commandBuffer, &renderInfo);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)swapchain.swapChainExtent.width;
        viewport.height = (float)swapchain.swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapchain.swapChainExtent;
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        return static_cast<int32_t>(imageIndex);
    }

    void waitForFence(VkDevice device, uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll, uint64_t timeout)
    {
        ZoneScopedN("vkWaitForFences");
        vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
    }

    VkResult acquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex)
    {
        ZoneScopedN("vkAcquireNextImageKHR");
        return vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
    }

    void drawCmdList(Camera &camera)
    {
        ZoneScoped; // PROFILER

        VkCommandBuffer cmd = commandBuffers[currentFrame];
        CameraPushConstant cameraData = {camera.getViewProj()};
        float zoom = camera.zoom;

        // Draw all instances
        for (const auto &dc : drawCmds)
        {
            assert(dc.vertexCount > 0 && "DrawCmd has zero vertexCount");
            assert(dc.instanceCount > 0 && "DrawCmd has zero instanceCount");
            assert(dc.firstVertex + dc.vertexCount <= vertexCapacity &&
                   "DrawCmd vertex range exceeds vertex buffer capacity!");
            assert(descriptorSets[(size_t)dc.atlasIndex] != VK_NULL_HANDLE && "Descriptor set missing!");

            const Pipeline &pipeline = pipelines[(size_t)dc.shaderType];
            FragPushConstant fragmentPushConstant = FragPushConstant{dc.atlasOffset, dc.atlasScale, camera.position, globalTime};
            const size_t constantSize = sizeof(FragPushConstant);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
            vkCmdBindDescriptorSets(cmd,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipeline.layout,
                                    0,
                                    1,
                                    &descriptorSets[(size_t)dc.atlasIndex],
                                    0,
                                    nullptr);
            vkCmdPushConstants(cmd,
                               pipeline.layout,
                               VK_SHADER_STAGE_VERTEX_BIT,
                               0,
                               sizeof(cameraData),
                               &cameraData);
            vkCmdPushConstants(cmd,
                               pipeline.layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(cameraData),
                               sizeof(fragmentPushConstant),
                               &fragmentPushConstant);

            // Bind vertex + instance buffer
            size_t instanceSize = sizeof(InstanceData);
            VkBuffer vertexBuffers[] = {vertexBuffer, instanceBuffer};
            VkDeviceSize offsets[] = {0, currentFrame * maxIntancesPerFrame * instanceSize};
            vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, offsets);

            // Issue draw
            vkCmdDraw(cmd, dc.vertexCount, dc.instanceCount, dc.firstVertex, dc.firstInstance);
        }

        // Draw particles
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, particleSystem.graphicsPipeline.pipeline);
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                particleSystem.graphicsPipeline.layout,
                                0,
                                1, &particleSystem.graphicsDescriptorSet,
                                0, nullptr);
        vkCmdPushConstants(cmd, particleSystem.graphicsPipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(glm::mat4),
                           &cameraData.viewProj);
        vkCmdPushConstants(cmd, particleSystem.graphicsPipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           sizeof(glm::mat4),
                           sizeof(float),
                           &zoom);

        vkCmdDraw(cmd, particleSystem.maxParticles, 1, 0, 0);
    }

    void endDraw(uint32_t imageIndex)
    {
        ZoneScoped; // PROFILER
        VkCommandBuffer commandBuffer = commandBuffers[currentFrame];

        vkCmdEndRendering(commandBuffer);
        barrierColorToPresent(swapchain, swapchainImageLayouts, imageIndex, commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer");
        }

        // --- Submit ---
        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapchain.handle};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        VkResult pres = vkQueuePresentKHR(presentQueue, &presentInfo);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapchain();
        }
        else if (pres != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image");
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void createStaticVertexBuffer()
    {
        const auto vertices = MeshRegistry::vertices;
        VkDeviceSize size = sizeof(Vertex) * vertices.size();

        CreateBuffer(device, physicalDevice,
                     size,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer,
                     vertexBufferMemory);

        void *data;
        vkMapMemory(device, vertexBufferMemory, 0, size, 0, &data);
        memcpy(data, vertices.data(), (size_t)size);
        vkUnmapMemory(device, vertexBufferMemory);

        vertexCapacity = static_cast<uint32_t>(vertices.size());
    }

    void uploadToInstanceBuffer()
    {
        // TODO  Rewrite instanceBuffer in Engine to use three seperate buffer based on the three different frames
        // that exist at the same time. This means we can allocate a new buffer without bothering the gpu. Pack it in a FrameResource.

        ZoneScoped; // PROFILER
        VkDeviceSize copySize = sizeof(InstanceData) * instances.size();

        // Resize if capacity too small
        if (instances.size() > maxIntancesPerFrame)
        {
            maxIntancesPerFrame = static_cast<uint32_t>(instances.size() * 5);
            VkDeviceSize stride = maxIntancesPerFrame * sizeof(InstanceData);
            VkDeviceSize totalSize = stride * MAX_FRAMES_IN_FLIGHT;

            if (instanceBufferMemory)
            {
                awaitDeviceIdle(); // wait until GPU is done using the old one
                vkDestroyBuffer(device, instanceBuffer, nullptr);
                vkUnmapMemory(device, instanceBufferMemory);
                vkFreeMemory(device, instanceBufferMemory, nullptr);
            }

            CreateBuffer(
                device,
                physicalDevice,
                totalSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                instanceBuffer,
                instanceBufferMemory);

            // map once, persistently
            VkResult result = vkMapMemory(device, instanceBufferMemory, 0, totalSize, 0, &instanceBufferMapped);
            if (result != VK_SUCCESS)
            {
                fprintf(stderr, "FATAL: vkMapMemory failed during instance buffer creation (code %d)\n", result);
                abort();
            }
        }

        // Write to the correct frame slice
        VkDeviceSize stride = maxIntancesPerFrame * sizeof(InstanceData);
        assert(copySize <= stride && "instance data exceeds slice capacity");
        size_t frameOffset = currentFrame * stride;
        memcpy(static_cast<char *>(instanceBufferMapped) + frameOffset, instances.data(), static_cast<size_t>(copySize));
    }

    void destroyColorResources()
    {
        if (colorImageView)
            vkDestroyImageView(device, colorImageView, nullptr);
        if (colorImage)
            vkDestroyImage(device, colorImage, nullptr);
        if (colorImageMemory)
            vkFreeMemory(device, colorImageMemory, nullptr);

        colorImageView = VK_NULL_HANDLE;
        colorImage = VK_NULL_HANDLE;
        colorImageMemory = VK_NULL_HANDLE;
    }

    void recreateSwapchain()
    {
        int width = 0, height = 0;
        window.getFramebufferSize(width, height);
        while (width == 0 || height == 0)
        {
            window.getFramebufferSize(width, height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(device);

        destroySyncObjects();
        swapchain.cleanup(device);
        destroyColorResources();

        swapchain.create(physicalDevice, device, surface, &window);
        swapchainImageLayouts.assign(swapchain.swapChainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
        createColorResources();

        createSyncObjects();
        createImagesInFlight();
        currentFrame = 0;
    }
};