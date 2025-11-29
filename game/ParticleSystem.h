#pragma once

#include "Buffer.h"
#include "Pipelines.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>

// IMPORTANT These structs need to match the insides of comp_particle.comp
struct Particle
{
    glm::vec2 pos;
    glm::vec2 vel;
    float life;
    uint32_t alive;
};

struct SpawnData
{
    glm::vec2 pos;
    glm::vec2 forward;
    uint32_t amount;
};

struct ParticleSystem
{
    // Buffers
    VkBuffer particleBuffer = VK_NULL_HANDLE;
    VkBuffer spawnBuffer = VK_NULL_HANDLE;
    VkBuffer freeListBuffer = VK_NULL_HANDLE;
    VkBuffer freeListHeadBuffer = VK_NULL_HANDLE;
    VkBuffer debugBuffer = VK_NULL_HANDLE;

    // Memory
    VkDeviceMemory particleMemory = VK_NULL_HANDLE;
    VkDeviceMemory spawnMemory = VK_NULL_HANDLE;
    VkDeviceMemory freeListMemory = VK_NULL_HANDLE;
    VkDeviceMemory freeListHeadMemory = VK_NULL_HANDLE;
    VkDeviceMemory debugMemory = VK_NULL_HANDLE;

    void *spawnMapped = nullptr;
    void *freeListHeadMapped = nullptr;

    // Compute pipeline
    VkDescriptorSetLayout computeDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;
    VkPipeline computePipeline = VK_NULL_HANDLE;

    // Graphics pipeline
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
    Pipeline graphicsPipeline{}; // {VkPipeline, VkPipelineLayout}

    uint32_t maxParticles = 100'000;

    // -------------------------
    // COMPUTE PIPELINE
    // -------------------------
    void createComputePipeline(VkDevice &device, VkDeviceSize &particleSize)
    {
        // Descriptor set layout bindings
        VkDescriptorSetLayoutBinding particleBinding{};
        particleBinding.binding = 0;
        particleBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        particleBinding.descriptorCount = 1;
        particleBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding spawnBinding{};
        spawnBinding.binding = 1;
        spawnBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        spawnBinding.descriptorCount = 1;
        spawnBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding freeListBinding{};
        freeListBinding.binding = 2;
        freeListBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        freeListBinding.descriptorCount = 1;
        freeListBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding freeListHeadBinding{};
        freeListHeadBinding.binding = 3;
        freeListHeadBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        freeListHeadBinding.descriptorCount = 1;
        freeListHeadBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutBinding debugBinding{};
        debugBinding.binding = 4;
        debugBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        debugBinding.descriptorCount = 1;
        debugBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        std::array<VkDescriptorSetLayoutBinding, 5> bindings = {particleBinding, spawnBinding, freeListBinding, freeListHeadBinding, debugBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute descriptor set layout");
        }

        // Descriptor pool
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(bindings.size());

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute descriptor pool");
        }

        // Descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = computeDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &computeDescriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &computeDescriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate compute descriptor set");
        }

        // Buffer infos
        VkDescriptorBufferInfo particleInfo{};
        particleInfo.buffer = particleBuffer;
        particleInfo.offset = 0;
        particleInfo.range = particleSize;

        VkDescriptorBufferInfo spawnInfo{};
        spawnInfo.buffer = spawnBuffer;
        spawnInfo.offset = 0;
        spawnInfo.range = sizeof(SpawnData);

        VkDescriptorBufferInfo freeListInfo{};
        freeListInfo.buffer = freeListBuffer;
        freeListInfo.offset = 0;
        freeListInfo.range = sizeof(uint32_t) * maxParticles;

        VkDescriptorBufferInfo freeListHeadInfo{};
        freeListHeadInfo.buffer = freeListHeadBuffer;
        freeListHeadInfo.offset = 0;
        freeListHeadInfo.range = sizeof(uint32_t);

        VkDescriptorBufferInfo debugInfo{};
        debugInfo.buffer = debugBuffer;
        debugInfo.offset = 0;
        debugInfo.range = sizeof(float) * 4;

        std::array<VkWriteDescriptorSet, 5> writes{};

        // binding 0 - particles
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = computeDescriptorSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[0].descriptorCount = 1;
        writes[0].pBufferInfo = &particleInfo;

        // binding 1 - spawn
        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = computeDescriptorSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].descriptorCount = 1;
        writes[1].pBufferInfo = &spawnInfo;

        // binding 2 - freeList
        writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet = computeDescriptorSet;
        writes[2].dstBinding = 2;
        writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].descriptorCount = 1;
        writes[2].pBufferInfo = &freeListInfo;

        // binding 3 - freeListHead
        writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet = computeDescriptorSet;
        writes[3].dstBinding = 3;
        writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[3].descriptorCount = 1;
        writes[3].pBufferInfo = &freeListHeadInfo;

        // binding 4 - debug
        writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[4].dstSet = computeDescriptorSet;
        writes[4].dstBinding = 4;
        writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[4].descriptorCount = 1;
        writes[4].pBufferInfo = &debugInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

        // Push constants
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(float);

        // Pipeline layout
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &computeDescriptorSetLayout;
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &pushRange;

        if (vkCreatePipelineLayout(device, &layout, nullptr, &computePipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create compute pipeline layout");
        }

        // Compute pipeline
        std::vector<char> vertShaderCode = readFile("shaders/comp_particle.spv");
        VkShaderModule computeShader = createShaderModule(vertShaderCode, device);

        VkPipelineShaderStageCreateInfo stage{};
        stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stage.module = computeShader;
        stage.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = stage;
        pipelineInfo.layout = computePipelineLayout;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS)
        {
            vkDestroyShaderModule(device, computeShader, nullptr);
            throw std::runtime_error("failed to create compute pipeline");
        }

        vkDestroyShaderModule(device, computeShader, nullptr);
    }

    // -------------------------
    // GRAPHICS PIPELINE (Dynamic Rendering)
    // -------------------------
    void createGraphicsPipeline(VkDevice &device,
                                VkSampleCountFlagBits &msaaSamples,
                                SwapChain &swapchain)
    {
        // Binding 0 = particle buffer (storage buffer)
        VkDescriptorSetLayoutBinding particleBinding{};
        particleBinding.binding = 0;
        particleBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        particleBinding.descriptorCount = 1;
        particleBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &particleBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &graphicsDescriptorSetLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create particle graphics descriptor set layout");
        }

        // Pool
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &graphicsDescriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create particle graphics descriptor pool");
        }

        // Allocate descriptor set
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = graphicsDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &graphicsDescriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &graphicsDescriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to allocate particle graphics descriptor set");
        }

        // Update descriptor set
        VkDescriptorBufferInfo particleInfo{};
        particleInfo.buffer = particleBuffer;
        particleInfo.offset = 0;
        particleInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = graphicsDescriptorSet;
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &particleInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

        // Shaders
        std::vector<char> vertShaderCode = readFile("shaders/vert_particle.spv");
        std::vector<char> fragShaderCode = readFile("shaders/frag_particle.spv");

        VkShaderModule vertShaderModule = createShaderModule(vertShaderCode, device);
        VkShaderModule fragShaderModule = createShaderModule(fragShaderCode, device);

        VkPipelineShaderStageCreateInfo vertStage{};
        vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertStage.module = vertShaderModule;
        vertStage.pName = "main";

        VkPipelineShaderStageCreateInfo fragStage{};
        fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragStage.module = fragShaderModule;
        fragStage.pName = "main";

        VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

        // No vertex input – all from storage buffer
        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount = 0;
        vertexInput.pVertexBindingDescriptions = nullptr;
        vertexInput.vertexAttributeDescriptionCount = 0;
        vertexInput.pVertexAttributeDescriptions = nullptr;

        // Draw points, not triangles
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.0f;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

        VkPipelineMultisampleStateCreateInfo msaa{};
        msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        msaa.rasterizationSamples = msaaSamples;
        msaa.sampleShadingEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorAttachment{};
        colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                         VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT |
                                         VK_COLOR_COMPONENT_A_BIT;
        colorAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorAttachment;

        std::vector<VkDynamicState> dynamicStates =
            {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();

        // Push constants: mat4 viewProj + float zoom
        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushRange.offset = 0;
        pushRange.size = sizeof(glm::mat4) + sizeof(float);

        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &graphicsDescriptorSetLayout;
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &pushRange;

        VkPipelineLayout pipeLayout;
        if (vkCreatePipelineLayout(device, &layout, nullptr, &pipeLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create particle graphics pipeline layout");
        }

        // Dynamic rendering attachment info
        VkFormat colorFormat = swapchain.swapChainImageFormat;

        VkPipelineRenderingCreateInfoKHR renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachmentFormats = &colorFormat;

        // Graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &msaa;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = pipeLayout;
        pipelineInfo.renderPass = VK_NULL_HANDLE; // dynamic rendering
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.pNext = &renderInfo;

        VkPipeline vkPipeline;
        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create particle graphics pipeline");
        }

        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        vkDestroyShaderModule(device, fragShaderModule, nullptr);

        graphicsPipeline = Pipeline{vkPipeline, pipeLayout};
    }

    // -------------------------
    // INIT
    // -------------------------
    void init(VkDevice &device, VkPhysicalDevice &physicalDevice, VkSampleCountFlagBits &msaaSamples, SwapChain &swapchain)
    {
        // Free list buffer
        CreateBuffer(device,
                     physicalDevice,
                     sizeof(uint32_t) * maxParticles,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     freeListBuffer,
                     freeListMemory);

        void *freeListMapped = nullptr;
        vkMapMemory(device, freeListMemory, 0, sizeof(uint32_t) * maxParticles, 0, &freeListMapped);
        auto *list = reinterpret_cast<uint32_t *>(freeListMapped);
        for (uint32_t i = 0; i < maxParticles; i++)
            list[i] = i;
        vkUnmapMemory(device, freeListMemory);

        // Free list head buffer
        CreateBuffer(device,
                     physicalDevice,
                     sizeof(uint32_t),
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     freeListHeadBuffer,
                     freeListHeadMemory);

        vkMapMemory(device, freeListHeadMemory, 0, sizeof(uint32_t), 0, &freeListHeadMapped);
        *(uint32_t *)freeListHeadMapped = maxParticles;

        // Debug buffer
        CreateBuffer(device,
                     physicalDevice,
                     sizeof(float) * 4,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     debugBuffer,
                     debugMemory);

        // Particle buffer
        VkDeviceSize particleSize = sizeof(Particle) * maxParticles;
        CreateBuffer(device,
                     physicalDevice,
                     particleSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     particleBuffer,
                     particleMemory);

        void *mapped = nullptr;
        vkMapMemory(device, particleMemory, 0, particleSize, 0, &mapped);
        memset(mapped, 0, static_cast<size_t>(particleSize));
        vkUnmapMemory(device, particleMemory);

        // Spawn buffer
        VkDeviceSize spawnSize = sizeof(SpawnData);
        CreateBuffer(device,
                     physicalDevice,
                     spawnSize,
                     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     spawnBuffer,
                     spawnMemory);

        vkMapMemory(device, spawnMemory, 0, spawnSize, 0, &spawnMapped);
        {
            auto *sd = reinterpret_cast<SpawnData *>(spawnMapped);
            sd->pos = glm::vec2(0.0f);
            sd->forward = glm::vec2(0.0f);
            sd->amount = 0;
        }

        // Pipelines
        createComputePipeline(device, particleSize);
        createGraphicsPipeline(device, msaaSamples, swapchain);
    }

    // -------------------------
    // DEBUG
    // -------------------------
    void debugPrintParticles(VkDevice device)
    {
        float *dbg = nullptr;
        vkMapMemory(device, debugMemory, 0, sizeof(float) * 4, 0, (void **)&dbg);
        printf("%f %f %f %f\n", dbg[0], dbg[1], dbg[2], dbg[3]);
        vkUnmapMemory(device, debugMemory);
    }

    // -------------------------
    // SPAWN / COMPUTE
    // -------------------------
    void updateSpawnFlag(glm::vec2 pos, glm::vec2 forward, uint32_t amount)
    {
        if (!spawnMapped)
            return;

        auto *sd = reinterpret_cast<SpawnData *>(spawnMapped);
        sd->pos = pos;
        sd->forward = forward;
        sd->amount = amount;
    }

    void recordCompute(VkCommandBuffer cmd, float delta)
    {
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

        vkCmdPipelineBarrier(
            cmd,
            VK_PIPELINE_STAGE_HOST_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            1, &barrier,
            0, nullptr,
            0, nullptr);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(cmd,
                                VK_PIPELINE_BIND_POINT_COMPUTE,
                                computePipelineLayout,
                                0, 1,
                                &computeDescriptorSet,
                                0, nullptr);

        vkCmdPushConstants(cmd, computePipelineLayout,
                           VK_SHADER_STAGE_COMPUTE_BIT,
                           0,
                           sizeof(float),
                           &delta);

        uint32_t localSize = 256;
        uint32_t groupCount = (maxParticles + localSize - 1) / localSize;
        vkCmdDispatch(cmd, groupCount, 1, 1);
    }

    void resetSpawn()
    {
        if (!spawnMapped)
            return;

        auto *sd = reinterpret_cast<SpawnData *>(spawnMapped);
        sd->amount = 0;
    }
};
