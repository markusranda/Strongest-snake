#pragma once

#include "components/Transform.h"
#include "components/Material.h"
#include "components/Renderable.h"
#include "Window.h"
#include "InstanceData.h"
#include "Logrador.h"
#include "Vertex.h"
#include "MeshRegistry.h"
#include "Pipelines.h"
#include "DrawCmd.h"
#include "EntityManager.h"
#include "Camera.h"
#include "Texture.h"
#include "Pipelines.h"
#include "Atlas.h"
#include "ParticleSystem.h"
#include "Text.h"
#include "RendererSwapchain.h"
#include "RendererBarriers.h"
#include "RendererSempahores.h"
#include "RendererApplication.h"
#include "RendererInstanceStorage.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <array>

// PROFILING
#include "tracy/Tracy.hpp"

const int MAX_FRAMES_IN_FLIGHT = 3;

struct Renderer
{
    Renderer(uint32_t width, uint32_t height, Window &window) : width(width), height(height), window(window) {}

    EntityManager ecs;
    uint32_t width;
    uint32_t height;
    Window window;

    RendererApplication application;
    RendererSempahores semaphores;
    RendererInstanceStorage instanceStorage;

    uint32_t currentFrame = 0;

    RendererSwapchain swapchain;
    std::vector<VkImageLayout> swapchainImageLayouts;

    std::array<Pipeline, static_cast<size_t>(ShaderType::COUNT)> pipelines;

    // Particle system
    ParticleSystem particleSystem;

    // MSAA
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;

    // Command
    std::vector<VkCommandBuffer> commandBuffers;

    // Instances
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceBufferMemory = VK_NULL_HANDLE;
    void *instanceBufferMapped = nullptr;
    uint32_t maxIntancesPerFrame = 0;

    // Vertices
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    uint32_t vertexCapacity = 0;

    // Texture
    VkDescriptorSetLayout textureSetLayout;
    AtlasRegion *atlasRegions = new AtlasRegion[MAX_ATLAS_ENTRIES];
    VkDescriptorPool descriptorPool;
    std::array<VkDescriptorSet, 2> descriptorSets;

    float globalTime = 0.0f;

    void init()
    {
        try
        {
            Logrador::info("Renderer is being created");
            createApplication();
            createSwapChain();
            createColorResources();
            createGraphicsPipeline();
            createAtlasData();
            createDescriptorPool();
            createDescriptorSets();
            createStaticVertexBuffer();
            createSemaphores();
            createCommandBuffers();
            createParticleSystem();
            Logrador::info("Renderer is complete");
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

    void createApplication()
    {
        application = RendererApplication(window.handle, swapchain);
    }

    void createSwapChain()
    {
        swapchain.create(
            application.physicalDevice,
            application.device,
            application.surface,
            &window);
        swapchainImageLayouts.assign(swapchain.swapChainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    }

    void createColorResources()
    {
        VkFormat colorFormat = swapchain.swapChainImageFormat;

        CreateImage(
            application.device,
            application.physicalDevice,
            swapchain.swapChainExtent.width,
            swapchain.swapChainExtent.height,
            application.msaaSamples,
            colorFormat,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            colorImage,
            colorImageMemory);

        colorImageView = CreateImageView(application.device, colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
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

        if (vkCreateDescriptorPool(application.device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
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

        if (vkAllocateDescriptorSets(application.device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor set!");

        VkDescriptorImageInfo fontInfo{};
        fontInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        fontInfo.imageView = application.fontTexture.view;
        fontInfo.sampler = application.fontTexture.sampler;

        VkDescriptorImageInfo atlasInfo{};
        atlasInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        atlasInfo.imageView = application.atlasTexture.view;
        atlasInfo.sampler = application.atlasTexture.sampler;

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

        vkUpdateDescriptorSets(application.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
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

        vkCreateDescriptorSetLayout(application.device, &layoutInfo, nullptr, &textureSetLayout);

        pipelines = CreateGraphicsPipelines(application.device, textureSetLayout, swapchain, application.msaaSamples);
    }

    void createSemaphores()
    {
        semaphores.init(application.device, swapchain, MAX_FRAMES_IN_FLIGHT);
    }

    void createCommandBuffers()
    {
        commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = application.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(application.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createParticleSystem()
    {
        particleSystem.init(application.device, application.physicalDevice, application.msaaSamples, swapchain);
    }

    // void buildTextInstances(float fps, glm::vec2 &playerCoords)
    // {
    //     ZoneScoped;

    //     {
    //         char text[32];
    //         std::snprintf(text, sizeof(text), "FPS: %d", (int)fps);
    //         uint32_t offset = instances.size();
    //         createTextInstances(text, sizeof(text), glm::vec2(-1.0f, -1.0f), instances);
    //         uint32_t count = instances.size() - offset;

    //         DrawCmd dc(
    //             UINT64_MAX,
    //             RenderLayer::World, // Add new layer for UI
    //             ShaderType::Font,
    //             0.0f, 0,
    //             6, 0,
    //             count,
    //             offset,
    //             AtlasIndex::Font,
    //             glm::vec2{},
    //             glm::vec2{});
    //         drawCmds.emplace_back(dc);
    //     }

    //     {
    //         char text[32];
    //         std::snprintf(text, sizeof(text), "POS: (%d, %d)", (int)playerCoords.x, (int)playerCoords.y);
    //         uint32_t offset = instances.size();
    //         createTextInstances(text, sizeof(text), glm::vec2(-1.0f, 0.9f), instances);
    //         uint32_t count = instances.size() - offset;

    //         DrawCmd dc(
    //             UINT64_MAX - 1,
    //             RenderLayer::World, // Add new layer for UI
    //             ShaderType::Font,
    //             0.0f, 0,
    //             6, 0,
    //             count,
    //             offset,
    //             AtlasIndex::Font,
    //             glm::vec2{},
    //             glm::vec2{});
    //         drawCmds.emplace_back(dc);
    //     }
    // }

    // void buildDebugChunkInstances()
    // {
    //     ZoneScoped;

    //     uint32_t offset = instances.size();
    //     ecs.collectChunkDebugInstances(instances);
    //     uint32_t count = instances.size() - offset;

    //     DrawCmd dc(
    //         UINT64_MAX - 2,
    //         RenderLayer::World,
    //         ShaderType::Border, // or a basic flat color shader
    //         0.0f, 0,
    //         6, 0,
    //         count,
    //         offset,
    //         AtlasIndex::Sprite, // or whatever fits your pipeline
    //         glm::vec2{},
    //         glm::vec2{});
    //     drawCmds.emplace_back(dc);
    // }

    uint32_t prepareDraw(float delta)
    {
        ZoneScoped;
        uint32_t imageIndex = semaphores.acquireImageIndex(application.device, currentFrame, swapchain);
        if (imageIndex == UINT32_MAX)
            return imageIndex;

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

        return imageIndex;
    }

    void drawCmdList(Camera &camera)
    {
        ZoneScoped;

        VkCommandBuffer cmd = commandBuffers[currentFrame];
        CameraPushConstant cameraData = {camera.getViewProj()};
        FragPushConstant fragmentPushConstant = FragPushConstant{camera.position, globalTime};
        float zoom = camera.zoom;

        // Draw all instances
        int instanceOffset = 0;
        for (const auto &dc : instanceStorage.drawCmds)
        {
            assert(dc.vertexCount > 0 && "DrawCmd has zero vertexCount");
            assert(dc.instanceCount > 0 && "DrawCmd has zero instanceCount");
            assert(dc.firstVertex + dc.vertexCount <= vertexCapacity &&
                   "DrawCmd vertex range exceeds vertex buffer capacity!");
            assert(descriptorSets[(size_t)dc.atlasIndex] != VK_NULL_HANDLE && "Descriptor set missing!");

            const Pipeline &pipeline = pipelines[(size_t)dc.shaderType];
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
            vkCmdDraw(cmd, dc.vertexCount, dc.instanceCount, dc.firstVertex, instanceOffset);
            instanceOffset += dc.instanceCount;
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
        ZoneScoped;
        VkCommandBuffer commandBuffer = commandBuffers[currentFrame];

        vkCmdEndRendering(commandBuffer);
        barrierColorToPresent(swapchain, swapchainImageLayouts, imageIndex, commandBuffer);
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
            throw std::runtime_error("failed to record command buffer");

        VkResult pres = semaphores.submitEndDraw(swapchain, currentFrame, commandBuffers, application.queue, imageIndex);
        if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR)
            recreateSwapchain();
        else
            assert(pres == VK_SUCCESS);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void createStaticVertexBuffer()
    {
        const auto vertices = MeshRegistry::vertices;
        VkDeviceSize size = sizeof(Vertex) * vertices.size();

        CreateBuffer(application.device,
                     application.physicalDevice,
                     size,
                     VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     vertexBuffer,
                     vertexBufferMemory);

        void *data;
        vkMapMemory(application.device, vertexBufferMemory, 0, size, 0, &data);
        memcpy(data, vertices.data(), (size_t)size);
        vkUnmapMemory(application.device, vertexBufferMemory);

        vertexCapacity = static_cast<uint32_t>(vertices.size());
    }

    void uploadToInstanceBuffer()
    {
        ZoneScoped;
        // TODO  Rewrite instanceBuffer in Renderer to use three seperate buffer based on the three different frames
        // that exist at the same time. This means we can allocate a new buffer without bothering the gpu. Pack it in a FrameResource.
        VkDeviceSize stride = maxIntancesPerFrame * sizeof(InstanceData);

        // Resize if capacity too small
        if (instanceStorage.instanceCount > maxIntancesPerFrame)
        {
            maxIntancesPerFrame = static_cast<uint32_t>(instanceStorage.instanceCount * 5);
            stride = maxIntancesPerFrame * sizeof(InstanceData);
            VkDeviceSize totalSize = stride * MAX_FRAMES_IN_FLIGHT;

            if (instanceBufferMemory)
            {
                application.deviceWaitIdle(); // wait until GPU is done using the old one
                vkDestroyBuffer(application.device, instanceBuffer, nullptr);
                vkUnmapMemory(application.device, instanceBufferMemory);
                vkFreeMemory(application.device, instanceBufferMemory, nullptr);
            }

            CreateBuffer(
                application.device,
                application.physicalDevice,
                totalSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                instanceBuffer,
                instanceBufferMemory);

            // map once, persistently
            VkResult result = vkMapMemory(application.device, instanceBufferMemory, 0, totalSize, 0, &instanceBufferMapped);
            if (result != VK_SUCCESS)
            {
                fprintf(stderr, "FATAL: vkMapMemory failed during instance buffer creation (code %d)\n", result);
                abort();
            }
        }

        // Write to the correct frame slice
        size_t frameOffset = currentFrame * stride;
        instanceStorage.uploadToGPUBuffer(static_cast<char *>(instanceBufferMapped) + frameOffset, stride);
    }

    void destroyColorResources()
    {
        if (colorImageView)
            vkDestroyImageView(application.device, colorImageView, nullptr);
        if (colorImage)
            vkDestroyImage(application.device, colorImage, nullptr);
        if (colorImageMemory)
            vkFreeMemory(application.device, colorImageMemory, nullptr);

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

        application.deviceWaitIdle();

        semaphores.destroySemaphores(application.device, MAX_FRAMES_IN_FLIGHT);
        swapchain.cleanup(application.device);
        destroyColorResources();

        swapchain.create(application.physicalDevice, application.device, application.surface, &window);
        swapchainImageLayouts.assign(swapchain.swapChainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
        createColorResources();

        semaphores.init(application.device, swapchain, MAX_FRAMES_IN_FLIGHT);
        currentFrame = 0;
    }

    void draw(Camera &camera, float fps, glm::vec2 playerCoords, float delta)
    {
        ZoneScoped;

        uint32_t imageIndex = prepareDraw(delta);
        if (imageIndex == UINT32_MAX)
        {
            recreateSwapchain();
            Logrador::debug("Skipping draw : Recreating swapchain");
            return;
        }

        //  Upload once, then draw all
        uploadToInstanceBuffer();
        drawCmdList(camera);
        endDraw((uint32_t)imageIndex);

        FrameMark;
    }
};