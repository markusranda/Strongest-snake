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
#include "RendererSwapchain.h"
#include "RendererBarriers.h"
#include "RendererSempahores.h"
#include "RendererApplication.h"
#include "RendererInstanceStorage.h"
#include "RendererUISystem.h"
#include "contexts/FrameCtx.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <array>

// PROFILING
#ifdef _DEBUG
#include "tracy/Tracy.hpp"
#include <tracy/TracyVulkan.hpp>

VkCommandPool tracyCmdPool = VK_NULL_HANDLE;
VkCommandBuffer tracyCmdBuf = VK_NULL_HANDLE;
TracyVkCtx tracyCtx = nullptr;

void InitTracyVulkan(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue, uint32_t queueFamilyIndex) {
    const VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex
    };

    vkCreateCommandPool(device, &poolInfo, nullptr, &tracyCmdPool);

    const VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = tracyCmdPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    vkAllocateCommandBuffers(device, &allocInfo, &tracyCmdBuf);

    tracyCtx = TracyVkContext(
        physicalDevice,
        device,
        queue,
        tracyCmdBuf
    );
}
#endif


inline static void crash(const char *msg) {
    fprintf(stderr, msg);
    abort();
}

const int MAX_FRAMES_IN_FLIGHT = 3;

struct GpuExecutor
{
    RendererUISystem uiSystem;
    Window *window;

    RendererApplication application;
    RendererSempahores semaphores;
    RendererInstanceStorage instanceStorage;

    uint32_t currentFrame = 0;

    RendererSwapchain swapchain;
    std::vector<VkImageLayout> swapchainImageLayouts;

    std::array<Pipeline, (size_t)ShaderType::COUNT> pipelines;

    // Particle system
    ParticleSystem particleSystem;

    // MSAA
    VkImage colorImage;
    VkDeviceMemory colorImageMemory;
    VkImageView colorImageView;

    // Command
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];

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

    void init() {
        try {
            Logrador::info("Renderer is being created");            
            application = CreateRendererApplication(window->handle, swapchain);
            createSwapChain();
            createColorResources();
            createGraphicsPipeline();
            createAtlasData();
            createDescriptorPool();
            createDescriptorSets();
            createStaticVertexBuffer();
            createSemaphores();
            createCommandBuffers();
            particleSystem = CreateParticleSystem(application, swapchain);
            createUiSystem();
            createInstanceStorage();

            #ifdef _DEBUG
                InitTracyVulkan(application.physicalDevice, application.device, application.queue, application.queueFamilyIndex);
            #endif

            Logrador::info("Renderer is complete");
        }
        catch (const std::exception &e) {
            throw std::runtime_error(std::string("Failed to construct engine: ") + e.what());
        }
        catch (...) {
            throw std::runtime_error(std::string("Unknown exception thrown in init: "));
        }
    }

    void createSwapChain() {
        swapchain.create(application.physicalDevice, application.device, application.surface, window);
        swapchainImageLayouts.assign(swapchain.swapChainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    }

    void createColorResources() {
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

    void createAtlasData() {
        std::ifstream in("assets/atlas.rigdb", std::ios::binary);
        if (!in)
            throw std::runtime_error("Failed to open atlas db file");
        char buffer[sizeof(AtlasRegion)];
        while (in.read(buffer, sizeof(buffer)))
        {
            AtlasRegion region = reinterpret_cast<AtlasRegion &>(buffer);
            atlasRegions[region.id] = region;
        }
    }

    void createDescriptorPool() {
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

    void createDescriptorSets() {
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

    void createGraphicsPipeline() {
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

    void createSemaphores() {
        semaphores.init(application.device, swapchain, MAX_FRAMES_IN_FLIGHT);
    }

    void createCommandBuffers() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = application.commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

        if (vkAllocateCommandBuffers(application.device, &allocInfo, commandBuffers) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void createUiSystem() {
        uiSystem.init(application, swapchain, atlasRegions);
    }

    void createInstanceStorage() {
        instanceStorage.init();
    }

    void recordInstanceDrawCmds(FrameCtx &ctx, float globalTime) {
        #ifdef _DEBUG
        ZoneScoped;
        TracyVkZone(tracyCtx, ctx.cmd, "Instance draw cmds");
        #endif

        VkBuffer buffers[] = { vertexBuffer, instanceBuffer };
        CameraPushConstant cameraData = { .viewProj = ctx.camera.getViewProj() };
        FragPushConstant fragmentPushConstant = FragPushConstant{ .cameraWorldPos = ctx.camera.position, .globalTime = globalTime };
        float zoom = ctx.camera.zoom;

        // Draw all instances
        int instanceOffset = 0;
        for (const auto &dc : instanceStorage.drawCmds)
        {
            assert(dc.vertexCount > 0 && "DrawCmd has zero vertexCount");
            assert(dc.instanceCount > 0 && "DrawCmd has zero instanceCount");
            assert(dc.firstVertex + dc.vertexCount <= vertexCapacity && "DrawCmd vertex range exceeds vertex buffer capacity!");

            VkDescriptorSet &descriptorSet = descriptorSets[(size_t)dc.atlasIndex];
            assert(descriptorSet != VK_NULL_HANDLE && "Descriptor set missing!");

            const Pipeline &pipeline = pipelines[(size_t)dc.shaderType];
            vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipeline);
            vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.layout, 0, 1, &descriptorSet, 0, nullptr);
            vkCmdPushConstants(ctx.cmd, pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(cameraData), &cameraData);
            vkCmdPushConstants(ctx.cmd, pipeline.layout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(cameraData), sizeof(fragmentPushConstant), &fragmentPushConstant);

            // Bind buffers
            VkDeviceSize offsets[] = {0, currentFrame * maxIntancesPerFrame * sizeof(InstanceData)};
            vkCmdBindVertexBuffers(ctx.cmd, 0, 2, buffers, offsets);

            // Issue cmd
            vkCmdDraw(ctx.cmd, dc.vertexCount, dc.instanceCount, dc.firstVertex, instanceOffset);
            instanceOffset += dc.instanceCount;
        }
    }

    void createStaticVertexBuffer() {
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

    void uploadToInstanceBuffer() {
        #ifdef _DEBUG
        ZoneScoped;
        #endif
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

    void destroyColorResources() {
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

    void recreateSwapchain() {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        int width = 0, height = 0;
        window->getFramebufferSize(width, height);
        while (width == 0 || height == 0)
        {
            window->getFramebufferSize(width, height);
            glfwWaitEvents();
        }

        application.deviceWaitIdle();

        semaphores.destroySemaphores(application.device, MAX_FRAMES_IN_FLIGHT);
        swapchain.cleanup(application.device);
        destroyColorResources();

        swapchain.create(application.physicalDevice, application.device, application.surface, window);
        swapchainImageLayouts.assign(swapchain.swapChainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);
        createColorResources();

        semaphores.init(application.device, swapchain, MAX_FRAMES_IN_FLIGHT);
        currentFrame = 0;
    }

    void recordCommands(Camera &camera, float globalTime, float delta) {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

        // Figure out if we do normal frame or recreate swapchain
        uint32_t imageIndex = semaphores.acquireImageIndex(application.device, currentFrame, swapchain);
        if (imageIndex == UINT32_MAX) {
            recreateSwapchain();
            Logrador::debug("Skipping draw : Recreating swapchain");
            return;
        }
        
        // Initialize a new commandBuffer
        VkCommandBuffer cmd = commandBuffers[currentFrame];
        VkCommandBufferBeginInfo beginInfo = { .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        
        vkResetCommandBuffer(cmd, 0);
        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
            crash("failed to begin command buffer");

        FrameCtx frameCtx = {
            .cmd = cmd,
            .camera = camera,
            .extent = glm::vec2{ swapchain.swapChainExtent.width, swapchain.swapChainExtent.height },
            .imageIndex = imageIndex,
            .delta = delta,
            #ifdef _DEBUG
            .tracyVkCtx = tracyCtx,
            #endif
        };

        // --- Record compute cmds ---
        particleSystem.recordSimCmds(frameCtx);
        
        // Prepare instance buffer
        uploadToInstanceBuffer();

        // --- Barrier ---
        barrierPresentToColor(swapchain, swapchainImageLayouts, imageIndex, cmd);
        // Dynamic rendering setup (MSAA color + resolve to swapchain)
        VkRenderingAttachmentInfoKHR colorAttachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
            .imageView = colorImageView, // MSAA image
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT,
            .resolveImageView = swapchain.swapChainImageViews[imageIndex],
            .resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = { .color = {0.5f, 0.5f, 0.5f, 1.0f} }
        };

        // --- Create render info ---
        VkRenderingInfoKHR renderInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
            .renderArea = {
                .offset = {0, 0},
                .extent = swapchain.swapChainExtent,
            },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachment,
        };
        vkCmdBeginRendering(cmd, &renderInfo);
        
        // --- Create viewport ---
        VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float)swapchain.swapChainExtent.width,
            .height = (float)swapchain.swapChainExtent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        // --- Create scissor ---
        VkRect2D scissor = { .offset = {0, 0}, .extent = swapchain.swapChainExtent };
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        
        // --- Record command for drawing ---
        recordInstanceDrawCmds(frameCtx, globalTime);
        uiSystem.recordDrawCmds(frameCtx);
        particleSystem.recordDrawCmds(frameCtx);

        // --- End command buffer and rendering ---
        vkCmdEndRendering(cmd);
        barrierColorToPresent(swapchain, swapchainImageLayouts, imageIndex, cmd);
        
        #ifdef _DEBUG
            // TRACY COLLECT
            TracyVkCollect(tracyCtx, cmd);
        #endif

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
            crash("failed to record command buffer");

        VkResult result = semaphores.submitEndDraw(swapchain, currentFrame, &cmd, application.queue, imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
        }
        else if (result != VK_SUCCESS) {
            crash("submit/preset failed");
        }
        
        // Figure out next frame
        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }
};
