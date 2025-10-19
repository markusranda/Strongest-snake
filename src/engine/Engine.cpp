#include "Engine.h"

// PROFILING
#include "tracy/Tracy.hpp"

// TODO LIST:
// - Make sure that this engine supports resize of window

Engine::Engine(uint32_t width, uint32_t height, Window &window) : width(width), height(height), window(window) {}

void Engine::initVulkan(std::string texturePath)
{
    try
    {
        Logger::info("Engine is being created");
        createInstance();
        createDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createCommandPool();
        createSwapChain();
        createRenderPass();
        createGraphicsPipeline();
        createTexture(texturePath);
        createDescriptorPool();
        createDescriptorSet();
        createStaticVertexBuffer();
        createFramebuffers();
        createImagesInFlight();
        createCommandBuffers();
        createSyncObjects();
        Logger::info("Engine is complete");
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(std::string("Failed to construct engine: ") + e.what());
    }
    catch (...)
    {
        throw std::runtime_error(std::string("Unknown exception thrown in Engine::initVulkan: "));
    }
}

void Engine::awaitDeviceIdle()
{
    ZoneScopedN("Awaiting device idle");
    vkDeviceWaitIdle(device);
}

void Engine::createInstance()
{
    if (enableValidationLayers && !checkValidationLayerSupport())
    {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
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

void Engine::createDebugMessenger()
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

void Engine::createSurface()
{
    auto status = glfwCreateWindowSurface(instance, window.getHandle(), nullptr, &surface);
    if (status != VK_SUCCESS)
    {
        char buf[128]; // plenty of space for your message
        snprintf(buf, sizeof(buf), "failed to create window surface: VkResult %d", status);
        std::string msg(buf);
        throw std::runtime_error(msg);
    }
}

void Engine::pickPhysicalDevice()
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

void Engine::createSwapChain()
{
    swapchain.create(
        physicalDevice,
        device,
        surface,
        &window,
        queueFamilies.graphicsFamily.value(),
        queueFamilies.presentFamily.value());
}

void Engine::createLogicalDevice()
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
}

void Engine::createTexture(std::string texturePath)
{
    fontTexture = LoadTexture(texturePath,
                              device,
                              physicalDevice,
                              commandPool,
                              graphicsQueue);
}

void Engine::createDescriptorPool()
{
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create descriptor pool!");
}

void Engine::createDescriptorSet()
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &textureSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate descriptor set!");

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = fontTexture.view;
    imageInfo.sampler = fontTexture.sampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

void Engine::createRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain.getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void Engine::createGraphicsPipeline()
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

    pipelines = CreateGraphicsPipelines(device, renderPass, textureSetLayout);
}

void Engine::createFramebuffers()
{
    swapchain.createFramebuffers(device, renderPass);
}

void Engine::createImagesInFlight()
{
    imagesInFlight.clear();
    imagesInFlight.resize(swapchain.getFramebuffers().size(), VK_NULL_HANDLE);
}

void Engine::createCommandPool()
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

void Engine::createCommandBuffers()
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

void Engine::createSyncObjects()
{
    size_t imageCount = swapchain.getFramebuffers().size();

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

void Engine::destroySyncObjects()
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

bool Engine::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        auto swapChainSupport = SwapChain::querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() &&
                            !swapChainSupport.presentModes.empty();
    }

    return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

bool Engine::checkDeviceExtensionSupport(VkPhysicalDevice device)
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

QueueFamilyIndices Engine::findQueueFamilies(VkPhysicalDevice device)
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

std::vector<const char *> Engine::getRequiredExtensions()
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

bool Engine::checkValidationLayerSupport()
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

void Engine::draw(Camera &camera, float fps)
{
    ZoneScoped; // PROFILER
    uint32_t imageIndex = prepareDraw();
    if (imageIndex < 0)
    {
        Logger::debug("Skipping draw : Recreating swapchain");
        return;
    }

    // Step 1: Sort entities
    {
        ZoneScopedN("Sort Entities");
        std::sort(ecs.renderables.begin(), ecs.renderables.end(), [](Renderable &a, Renderable &b)
                  { return a.drawkey < b.drawkey; });
    }
    // Step 2: Collect all instance data
    std::vector<DrawCmd> drawCmds;
    ShaderType currentShader = ShaderType::COUNT;
    RenderLayer currentLayer = RenderLayer::World;
    uint32_t instanceOffset = 0;

    {
        ZoneScopedN("Build world InstanceData");

        instances.clear();
        for (auto &renderable : ecs.renderables)
        {
            Quad &quad = ecs.quads[ecs.entityToQuad[entityIndex(renderable.entity)]];
            Transform &transform = ecs.transforms[ecs.entityToTransform[entityIndex(renderable.entity)]];
            bool newBatch = (quad.shaderType != currentShader) || (quad.renderLayer != currentLayer);

            if (newBatch && !instances.empty())
            {
                uint32_t count = instances.size() - instanceOffset;
                drawCmds.emplace_back(currentLayer,
                                      currentShader,
                                      quad.z,
                                      quad.tiebreak,
                                      6,
                                      0,
                                      count,
                                      instanceOffset);
                instanceOffset = instances.size();
            }
            InstanceData instance = {transform.model, quad.color};
            instances.push_back(instance);
            currentShader = quad.shaderType;
            currentLayer = quad.renderLayer;
        }
    }

    // Fetch all text instances
    {
        ZoneScopedN("Build text InstanceData");

        std::vector<InstanceData> textInstances = BuildTextInstances(
            "FPS: " + std::to_string(fps),
            glm::vec2(-1.0f, -1.0f));
        uint32_t textOffset = instances.size();
        instances.insert(instances.end(), textInstances.begin(), textInstances.end());
        DrawCmd dc(
            RenderLayer::World, // Add new layer for UI
            ShaderType::Font,
            0.0f, 0,
            6, 0,
            static_cast<uint32_t>(textInstances.size()),
            textOffset);
        drawCmds.emplace_back(dc);
    }

    // Last batch
    if (!instances.empty())
    {
        ZoneScopedN("Adding last batch of drawCmds");

        uint32_t count = instances.size() - instanceOffset;
        drawCmds.emplace_back(currentLayer,
                              currentShader,
                              0.0f,
                              0,
                              6,
                              0,
                              count,
                              instanceOffset);
    }

    // Step 3: Upload once, then draw all
    uploadToInstanceBuffer();
    drawCmdList(drawCmds, camera);
    endDraw(imageIndex);

    FrameMark;
}

uint32_t Engine::prepareDraw()
{
    ZoneScoped; // PROFILER
    waitForFence(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    uint32_t imageIndex;
    VkResult result = acquireNextImageKHR(
        device,
        swapchain.getHandle(),
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
    renderPassInfo.framebuffer = swapchain.getFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchain.getExtent();

    VkClearValue clearColor = {{{0.5f, 0.5f, 0.5f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swapchain.getExtent().width;
    viewport.height = (float)swapchain.getExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchain.getExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    return imageIndex;
}

void Engine::waitForFence(VkDevice device, uint32_t fenceCount, const VkFence *pFences, VkBool32 waitAll, uint64_t timeout)
{
    ZoneScopedN("vkWaitForFences");
    vkWaitForFences(device, fenceCount, pFences, waitAll, timeout);
}

VkResult Engine::acquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, uint64_t timeout, VkSemaphore semaphore, VkFence fence, uint32_t *pImageIndex)
{
    ZoneScopedN("vkAcquireNextImageKHR");
    return vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
}

void Engine::drawCmdList(const std::vector<DrawCmd> &drawCmds, Camera &camera)
{
    ZoneScoped; // PROFILER
    VkCommandBuffer cmd = commandBuffers[currentFrame];

    glm::mat4 viewProj = camera.getViewProj();

    for (const auto &dc : drawCmds)
    {
        const Pipeline &pipeline = pipelines[(size_t)dc.shaderType];

        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline.layout,
                                0,
                                1, &descriptorSet,
                                0, nullptr);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);

        vkCmdPushConstants(cmd,
                           pipeline.layout,
                           VK_SHADER_STAGE_VERTEX_BIT,
                           0,
                           sizeof(glm::mat4),
                           &viewProj);

        // Bind vertex + instance buffer
        VkBuffer vertexBuffers[] = {vertexBuffer, instanceBuffer};
        VkDeviceSize offsets[] = {0, currentFrame * maxIntancesPerFrame * sizeof(InstanceData)};
        vkCmdBindVertexBuffers(cmd, 0, 2, vertexBuffers, offsets);

        // Issue draw
        vkCmdDraw(cmd, dc.vertexCount, dc.instanceCount, dc.firstVertex, dc.firstInstance);
    }
}

void Engine::endDraw(uint32_t imageIndex)
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
    VkSwapchainKHR swapChains[] = {swapchain.getHandle()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Engine::createStaticVertexBuffer()
{
    const auto &unitVerts = Quad::getUnitVertices();
    VkDeviceSize size = sizeof(Vertex) * unitVerts.size();

    CreateBuffer(device, physicalDevice,
                 size,
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 vertexBuffer,
                 vertexBufferMemory);

    void *data;
    vkMapMemory(device, vertexBufferMemory, 0, size, 0, &data);
    memcpy(data, unitVerts.data(), (size_t)size);
    vkUnmapMemory(device, vertexBufferMemory);

    vertexCapacity = static_cast<uint32_t>(unitVerts.size());
}

void Engine::uploadToInstanceBuffer()
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
        assert(
            vkMapMemory(device, instanceBufferMemory, 0, totalSize, 0, &instanceBufferMapped) == VK_SUCCESS &&
            "failed to map memory during instance buffer creation");
    }

    // Write to the correct frame slice
    VkDeviceSize stride = maxIntancesPerFrame * sizeof(InstanceData);
    assert(copySize <= stride && "instance data exceeds slice capacity");
    size_t frameOffset = currentFrame * stride;

    memcpy(
        static_cast<char *>(instanceBufferMapped) + frameOffset,
        instances.data(),
        static_cast<size_t>(copySize));
}

void Engine::recreateSwapchain()
{
    ZoneScopedN("Sort Entities");
    Logger::info("Recreating swapchain");

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
    swapchain.createFramebuffers(device, renderPass);

    createSyncObjects();
    createImagesInFlight();
    currentFrame = 0;
}