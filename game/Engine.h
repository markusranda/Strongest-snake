#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Window.h"
#include "SwapChain.h"
#include "InstanceData.h"
#include "Logrador.h"
#include "Vertex.h"
#include "MeshRegistry.h"
#include "Pipelines.h"
#include "Draworder.h"
#include "EntityManager.h"
#include "Camera.h"
#include "Transform.h"
#include "Texture.h"
#include "Debug.h"
#include "Pipelines.h"
#include "Draworder.h"
#include "Atlas.h"
#include "FragPushConstant.h"
#include "ParticleSystem.h"
#include "SnakeMath.h"

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

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> graphicsAndComputeFamily;

    bool isComplete()
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

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
    VkQueue computeQueue;

    QueueFamilyIndices queueFamilies;

    SwapChain swapchain;

    VkRenderPass renderPass;
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

    // Vertices
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t vertexCapacity = 0;
    void *instanceBufferMapped = nullptr;

    // Texture
    VkDescriptorSetLayout textureSetLayout;
    Texture fontTexture;
    Texture atlasTexture;
    std::unordered_map<std::string, AtlasRegion> atlasRegions;
    VkDescriptorPool descriptorPool;
    std::array<VkDescriptorSet, 2> descriptorSets;

    float globalTime = 0.0f;

    void init(std::string atlasPath, std::string fontPath, std::string atlasDataPath)
    {
        try
        {
            Logrador::info("Engine is being created");
            createInstance();
            createDebugMessenger();
            createSurface();
            pickPhysicalDevice();
            pickMsaaSampleCount();
            createLogicalDevice();
            createCommandPool();
            createTextures(atlasPath, fontPath);
            createSwapChain();
            createColorResources();
            createRenderPass();
            createGraphicsPipeline();
            createAtlasData(atlasDataPath);
            createDescriptorPool();
            createDescriptorSets();
            createStaticVertexBuffer();
            createFramebuffers();
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
        appInfo.apiVersion = VK_API_VERSION_1_0;

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
            &window,
            queueFamilies.graphicsFamily.value(),
            queueFamilies.presentFamily.value());
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

    void createLogicalDevice()
    {
        queueFamilies = findQueueFamilies(physicalDevice);

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {queueFamilies.graphicsFamily.value(), queueFamilies.presentFamily.value()};

        float queuePriority = 1.0f;
        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

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

        vkGetDeviceQueue(device, queueFamilies.graphicsFamily.value(), 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueFamilies.presentFamily.value(), 0, &presentQueue);
        vkGetDeviceQueue(device, queueFamilies.graphicsAndComputeFamily.value(), 0, &computeQueue);
    }

    void createTextures(std::string atlasPath, std::string fontPath)
    {
        atlasTexture = LoadTexture(atlasPath,
                                   device,
                                   physicalDevice,
                                   commandPool,
                                   graphicsQueue);

        fontTexture = LoadTexture(fontPath,
                                  device,
                                  physicalDevice,
                                  commandPool,
                                  graphicsQueue);
    }

    void createAtlasData(std::string atlasDataPath)
    {
        std::ifstream in(atlasDataPath, std::ios::binary);
        if (!in)
        {
            throw std::runtime_error("Failed to open file");
        }
        char buffer[sizeof(AtlasRegion)];
        while (in.read(buffer, sizeof(buffer)))
        {
            AtlasRegion region = reinterpret_cast<AtlasRegion &>(buffer);
            atlasRegions[region.name] = region;
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

    void createRenderPass()
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapchain.swapChainImageFormat;
        colorAttachment.samples = msaaSamples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription colorAttachmentResolve{};
        colorAttachmentResolve.format = swapchain.swapChainImageFormat;
        colorAttachmentResolve.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentResolveRef{};
        colorAttachmentResolveRef.attachment = 1;
        colorAttachmentResolveRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pResolveAttachments = &colorAttachmentResolveRef;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, colorAttachmentResolve};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;

        if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
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

        pipelines = CreateGraphicsPipelines(device, renderPass, textureSetLayout, msaaSamples);
    }

    void createFramebuffers()
    {
        swapchain.createFramebuffers(device, renderPass, colorImageView, msaaSamples);
    }

    void createImagesInFlight()
    {
        imagesInFlight.clear();
        imagesInFlight.resize(swapchain.swapChainFramebuffers.size(), VK_NULL_HANDLE);
    }

    void createCommandPool()
    {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

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
        size_t imageCount = swapchain.swapChainFramebuffers.size();

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
        particleSystem.init(device, physicalDevice, commandPool, computeQueue, renderPass, msaaSamples);
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
        QueueFamilyIndices indices = findQueueFamilies(device);

        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported)
        {
            auto swapChainSupport = swapchain.querySwapChainSupport(device, surface);
            swapChainAdequate = !swapChainSupport.formats.empty() &&
                                !swapChainSupport.presentModes.empty();
        }

        return indices.isComplete() && extensionsSupported && swapChainAdequate;
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

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
                indices.graphicsAndComputeFamily = i;
            }

            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.isComplete())
            {
                break;
            }

            i++;
        }

        return indices;
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

    void draw(Camera &camera, float fps, glm::vec2 playerCoords)
    {
        ZoneScoped; // PROFILER

        int32_t imageIndex = prepareDraw();
        if (imageIndex < 0)
        {
            Logrador::debug("Skipping draw : Recreating swapchain");
            return;
        }

        // Collect all instance data
        std::vector<DrawCmd> drawCmds;
        {
            ZoneScopedN("Build world InstanceData");

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
            ecs.activeEntities.clear();

            float halfW = (camera.screenW * 0.5f) / camera.zoom;
            float halfH = (camera.screenH * 0.5f) / camera.zoom;
            AABB cameraBox{
                {camera.position.x - halfW, camera.position.y - halfH},
                {camera.position.x + halfW, camera.position.y + halfW},
            };
            ecs.getIntersectingEntities(cameraBox, ecs.activeEntities);
            {
                ZoneScopedN("Sorting renderables");
                std::sort(ecs.activeEntities.begin(), ecs.activeEntities.end(), [this](Entity &a, Entity &b)
                          { 
                    Renderable &rA = ecs.renderables[ecs.entityToRenderable[entityIndex(a)]];
                    Renderable &rB = ecs.renderables[ecs.entityToRenderable[entityIndex(b)]];
                    return rA.drawkey < rB.drawkey; });
            }

            if (ecs.activeEntities.empty())
                return;

            Renderable &firstRenderable =
                ecs.renderables[ecs.entityToRenderable[entityIndex(ecs.activeEntities[0])]];
            uint64_t prevDrawKey = firstRenderable.drawkey;
            for (Entity &entity : ecs.activeEntities)
            {
                uint32_t entityId = entityIndex(entity);
                Renderable renderable = ecs.renderables[ecs.entityToRenderable[entityId]];
                Mesh &mesh = ecs.meshes[ecs.entityToMesh[entityId]];
                Transform &transform = ecs.transforms[ecs.entityToTransform[entityId]];
                Material &material = ecs.materials[ecs.entityToMaterial[entityId]];
                glm::vec4 &uvTransform = ecs.uvTransforms[ecs.entityToUvTransforms[entityId]];
                glm::vec2 atlasOffset = glm::vec2{uvTransform.x, uvTransform.y};
                glm::vec2 atlasScale = glm::vec2{uvTransform.z, uvTransform.w};

                if (renderable.drawkey != prevDrawKey && !instances.empty())
                {
                    uint32_t instanceCount = instances.size() - instanceOffset;
                    DrawCmd drawCmd = DrawCmd(
                        currentLayer,
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
                    drawCmds.push_back(drawCmd);
                    instanceOffset = instances.size();
                }

                InstanceData instance = {transform.model, material.color, uvTransform};
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

        // Fetch all text instances
        {
            ZoneScopedN("Build text InstanceData");

            std::vector<InstanceData> textInstances = BuildTextInstances(
                "FPS: " + std::to_string((int)fps),
                glm::vec2(-1.0f, -1.0f));
            uint32_t textOffset = instances.size();
            instances.insert(instances.end(), textInstances.begin(), textInstances.end());
            DrawCmd dc = DrawCmd(
                RenderLayer::World, // Add new layer for UI
                ShaderType::Font,
                0.0f, 0,
                6, 0,
                static_cast<uint32_t>(textInstances.size()),
                textOffset,
                AtlasIndex::Font,
                glm::vec2{},
                glm::vec2{});
            drawCmds.emplace_back(dc);

            std::string coordText = "POS: " + std::to_string((float)playerCoords.x) + ", " + std::to_string((float)playerCoords.y);
            textOffset = instances.size();
            textInstances = BuildTextInstances(
                coordText,
                glm::vec2(-1.0f, 0.9f));
            instances.insert(instances.end(), textInstances.begin(), textInstances.end());
            dc = DrawCmd(
                RenderLayer::World, // Add new layer for UI
                ShaderType::Font,
                0.0f, 0,
                6, 0,
                static_cast<uint32_t>(textInstances.size()),
                textOffset,
                AtlasIndex::Font,
                glm::vec2{},
                glm::vec2{});
            drawCmds.emplace_back(dc);
        }

        // {
        //     ZoneScopedN("Debug QuadTree");

        //     std::vector<InstanceData> debugInstances;
        //     ecs.collectQuadTreeDebugInstances(ecs.aabbRoot, debugInstances);

        //     if (!debugInstances.empty())
        //     {
        //         uint32_t offset = instances.size();
        //         instances.insert(instances.end(), debugInstances.begin(), debugInstances.end());

        //         DrawCmd dc(
        //             RenderLayer::World,
        //             ShaderType::Border, // or a basic flat color shader
        //             0.0f, 0,
        //             6, 0,
        //             static_cast<uint32_t>(debugInstances.size()),
        //             offset,
        //             AtlasIndex::Sprite, // or whatever fits your pipeline
        //             glm::vec2{},
        //             glm::vec2{});
        //         drawCmds.emplace_back(dc);
        //     }
        // }

        for (auto &cmd : drawCmds)
            assert(cmd.firstInstance + cmd.instanceCount <= instances.size());

        //  Upload once, then draw all
        uploadToInstanceBuffer();
        drawCmdList(drawCmds, camera);
        endDraw(imageIndex);
        particleSystem.resetSpawn();

        FrameMark;
    }

    int32_t prepareDraw()
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

        // Mark this image as now being in use by this frame
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
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapchain.swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapchain.swapChainExtent;

        VkClearValue clearColor = {{{0.5f, 0.5f, 0.5f, 1.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        // Update particle system
        particleSystem.recordCompute(commandBuffer);

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
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

        return imageIndex;
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

    void drawCmdList(const std::vector<DrawCmd> &drawCmds, Camera &camera)
    {
        ZoneScoped; // PROFILER

        VkCommandBuffer cmd = commandBuffers[currentFrame];
        glm::mat4 viewProj = camera.getViewProj();
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
            FragPushConstant fragmentPushConstant = FragPushConstant{dc.atlasOffset, dc.atlasScale, globalTime};
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
                               sizeof(glm::mat4),
                               &viewProj);
            vkCmdPushConstants(cmd,
                               pipeline.layout,
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               sizeof(glm::mat4),
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
                           &viewProj);
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

        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);

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

        vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = {swapchain.handle};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(presentQueue, &presentInfo);

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

        vertexCapacity = static_cast<uint32_t>(size);
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

    void recreateSwapchain()
    {
        ZoneScopedN("Sort Entities");
        Logrador::info("Recreating swapchain");

        int width = 0, height = 0;
        window.getFramebufferSize(width, height);

        // Avoid recreating if minimized (some platforms give 0-size extents)
        while (width == 0 || height == 0)
        {
            window.getFramebufferSize(width, height);
            glfwWaitEvents(); // block until window is usable again
        }

        vkDeviceWaitIdle(device);
        destroySyncObjects();

        swapchain.cleanup(device); // kills framebuffers + image views + swapchain
        swapchain.create(physicalDevice, device, surface, &window,
                         queueFamilies.graphicsFamily.value(),
                         queueFamilies.presentFamily.value());
        swapchain.createFramebuffers(device, renderPass, colorImageView, msaaSamples);

        createSyncObjects();
        createImagesInFlight();
        currentFrame = 0;
    }
};