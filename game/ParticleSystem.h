#pragma once

#include "Buffer.h"
#include "Pipelines.h"
#include "RendererApplication.h"
#include "RendererSwapChain.h"
#include "contexts/FrameCtx.h"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <vector>
#include <array>

// PROFILING
#ifdef _DEBUG
#include "tracy/Tracy.hpp"
#include <tracy/TracyVulkan.hpp>
#endif

constexpr static float TIME_ACCUMULATOR_MAX = 1.0f / 60.0f; 
constexpr uint32_t LOCAL_SIZE = 64;
constexpr uint32_t MAX_PARTICLES = 100'000;
constexpr uint32_t MAX_SPAWN = 500;

// IMPORTANT These structs need to match the insides of comp_particle.comp
struct Particle
{
    glm::vec2 pos;
    glm::vec2 vel;
    float size;
    float _pad;
    float life;
    uint32_t alive;
};

struct SpawnData
{
    glm::vec2 pos;
    glm::vec2 forward;
    uint32_t spawnCount;
    uint32_t _pad;
};
static_assert(sizeof(SpawnData) == 24); // Update shaders if this fails
static_assert(offsetof(SpawnData, spawnCount) == 16);

struct Counters
{
    uint32_t inCount;
    uint32_t outCount;
    uint32_t maxParticles;
    uint32_t _pad;
};
struct CountersAndDrawCmd
{
    Counters counters;
    VkDrawIndirectCommand draw;
};
static_assert(sizeof(Counters) == 16); // Update shaders if this fails
static_assert(sizeof(CountersAndDrawCmd) == 32); // Update shaders if this fails

struct ParticleSystem
{   
    Pipeline computeSimPipeline;
    Pipeline computeSpawnPipeline;
    Pipeline computeCountersPipeline;
    Pipeline graphicsPipeline;

    // Mapped
    void *spawnMapped = nullptr;
    void *countersMapped = nullptr;
    
    // Buffer
    VkBuffer countersBuffer;

    // Index for ping pong buffers
    uint32_t descriptorSetSimIndex = 0;

    // Timers
    float timeAccumulator = 0.0f;

    void updateSpawnFlag(glm::vec2 pos, glm::vec2 forward, uint32_t amount) {
        uint32_t spawnCount = std::min(amount, MAX_SPAWN); // Cap it to max
        SpawnData *sd = reinterpret_cast<SpawnData *>(spawnMapped);
        sd->pos = pos;
        sd->forward = forward;
        sd->spawnCount = spawnCount;
    }

    void recordSimCmds(FrameCtx &ctx) {
        #ifdef _DEBUG 
        ZoneScoped;
        TracyVkZone(ctx.tracyVkCtx, ctx.cmd, "Particle simulation");
        #endif

        // Run at the rate of timeAccumulatorMax  
        if ((timeAccumulator += ctx.delta) < TIME_ACCUMULATOR_MAX) {
            return; // Skip if it's not time yet.
        }
        float simDelta = timeAccumulator; // Record how much time it took
        timeAccumulator = 0; // Reset timer
        uint32_t frameseed = std::min(256 * (uint32_t)simDelta, UINT32_MAX);

        // Pick correct descriptor set for this run
        descriptorSetSimIndex ^= 1;

        // Number of thread groupCountX needed to manage the number of particles
        // NOTICE: Needs to match shaders\comp_particle_spawn.comp and shaders\comp_particle_sim.comp
        uint32_t local_size_x = 64;
        CountersAndDrawCmd *counters = reinterpret_cast<CountersAndDrawCmd *>(countersMapped);
        uint32_t groupCountXsim = CeilDivision(counters->counters.inCount, local_size_x);
        uint32_t groupCountXspawn = CeilDivision(MAX_SPAWN, local_size_x);

        // ====================================
        // PASS ONE (SIMULATE):
        // ====================================
        vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeSimPipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeSimPipeline.layout, 0, 1, &computeSimPipeline.descriptorSet[descriptorSetSimIndex], 0, nullptr);
        vkCmdPushConstants(ctx.cmd, computeSimPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &simDelta);
        vkCmdDispatch(ctx.cmd, groupCountXsim, 1, 1);

        // BARRIER
        VkMemoryBarrier2 memBarrierA{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        };
        VkDependencyInfo depA{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &memBarrierA,
        };
        vkCmdPipelineBarrier2(ctx.cmd, &depA);

        // ====================================
        // PASS TWO (SPAWN):
        // ====================================
        vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeSpawnPipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeSpawnPipeline.layout, 0, 1, &computeSpawnPipeline.descriptorSet[descriptorSetSimIndex], 0, nullptr);
        vkCmdPushConstants(ctx.cmd, computeSpawnPipeline.layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &frameseed);
        vkCmdDispatch(ctx.cmd, groupCountXspawn, 1, 1);
        
        // BARRIER
        VkMemoryBarrier2 memBarrierB{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        };
        VkDependencyInfo depB{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &memBarrierB,
        };
        vkCmdPipelineBarrier2(ctx.cmd, &depB);

        // ====================================
        // PASS TWO (SPAWN):
        // ====================================
        vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeCountersPipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeCountersPipeline.layout, 0, 1, &computeCountersPipeline.descriptorSet[0], 0, nullptr);
        vkCmdDispatch(ctx.cmd, 1, 1, 1);

        VkMemoryBarrier2 toGraphics {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
            .srcStageMask  = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
            .dstStageMask  = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT,
        };
        VkDependencyInfo dep {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .memoryBarrierCount = 1,
            .pMemoryBarriers = &toGraphics,
        };

        vkCmdPipelineBarrier2(ctx.cmd, &dep);
    }

    // NOTICE: This has to be called after recordSimCmds
    void recordDrawCmds(FrameCtx &ctx) {
        #ifdef _DEBUG
        ZoneScoped;
        TracyVkZone(ctx.tracyVkCtx, ctx.cmd, "Particle draw");
        #endif 

        CameraPushConstant cameraData = { ctx.camera.getViewProj() };
        float zoom = ctx.camera.zoom;
        uint32_t drawIdx = 1u - descriptorSetSimIndex;
        
        vkCmdBindPipeline(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.pipeline);
        vkCmdBindDescriptorSets(ctx.cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline.layout, 0, 1, &graphicsPipeline.descriptorSet[drawIdx], 0, nullptr);
        vkCmdPushConstants(ctx.cmd, graphicsPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), &cameraData.viewProj);
        vkCmdPushConstants(ctx.cmd, graphicsPipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(glm::mat4), sizeof(float), &zoom);
        vkCmdDrawIndirect(ctx.cmd, countersBuffer, sizeof(Counters), 1, sizeof(VkDrawIndirectCommand));
    }
};

// ---------------------------------------------------------------------------------------------
// SIM PIPELINE
// ---------------------------------------------------------------------------------------------

static Pipeline CreateComputeSimPipeline(VkDevice &device, VkBuffer particleBuffer, VkBuffer countersBuffer) {
    // Descriptor set layout bindings
    VkDescriptorSetLayoutBinding particleBindingIn = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    VkDescriptorSetLayoutBinding particleBindingOut = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    VkDescriptorSetLayoutBinding counterBinding = {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    const uint32_t bindingsLen = 3;
    VkDescriptorSetLayoutBinding bindings[bindingsLen] = { particleBindingIn, particleBindingOut, counterBinding };
    VkDescriptorSetLayout computeSimDescriptorSetLayout = VK_NULL_HANDLE;
    CreateDescriptorSetLayout(device, bindings, bindingsLen, computeSimDescriptorSetLayout);

    // Push constants
    VkPushConstantRange pushRange = {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(float),
    };

    // Pipeline layout creation
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &computeSimDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange,
    };
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create compute pipeline layout");
    }

    // Descriptor pool
    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = bindingsLen * 2,
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create compute descriptor pool");

    // Allocation for descriptor sets
    VkDescriptorSetLayout setLayouts[2] = { computeSimDescriptorSetLayout, computeSimDescriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = setLayouts,
    };
    VkDescriptorSet descriptorSet[2];
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate compute descriptor sets");
    }

    // Descriptor sets for buffers
    VkDeviceSize halfSize = (MAX_PARTICLES / 2) * sizeof(Particle);
    VkDescriptorBufferInfo particleInfo[2] = {
        { .buffer = particleBuffer, .offset = 0, .range = halfSize },
        { .buffer = particleBuffer, .offset = halfSize, .range = halfSize },
    };
    VkDescriptorBufferInfo countersInfo{
        .buffer = countersBuffer,
        .offset = 0,
        .range = sizeof(CountersAndDrawCmd),
    };

    VkWriteDescriptorSet writes[2][3];
    for (size_t i = 0; i < 2; i++) {
        uint32_t inIdx  = i;
        uint32_t outIdx = 1 - i;

        // binding 0 - particles in
        writes[i][0] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &particleInfo[inIdx],
        };

        // binding 1 - particles out
        writes[i][1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet[i],
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &particleInfo[outIdx],
        };

        // binding 2 - counters
        writes[i][2] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSet[i],
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &countersInfo,
        };

        vkUpdateDescriptorSets(device, 3, writes[i], 0, nullptr);
    }

    

    return Pipeline { 
        .pipeline      = CreateComputePipeline(device, "shaders/comp_particle_sim.spv", pipelineLayout),
        .layout        = pipelineLayout,
        .descriptorSet = {
            descriptorSet[0],
            descriptorSet[1],
        }
    };
}

// ---------------------------------------------------------------------------------------------
// SPAWN PIPELINE
// ---------------------------------------------------------------------------------------------

static Pipeline CreateComputeSpawnPipeline(VkDevice &device, VkBuffer particleBuffer, VkBuffer spawnBuffer, VkBuffer countersBuffer) {
    // Descriptor set layout (set=0)
    VkDescriptorSetLayoutBinding outBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    VkDescriptorSetLayoutBinding countersBinding = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    VkDescriptorSetLayoutBinding spawnBinding = {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    const uint32_t bindingsLen = 3;
    VkDescriptorSetLayoutBinding bindings[bindingsLen] = { outBinding, countersBinding, spawnBinding };
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    CreateDescriptorSetLayout(device, bindings, bindingsLen, descriptorSetLayout);

    // Push constant
    VkPushConstantRange pushRange = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = sizeof(uint32_t) };

    // Pipeline layout
    VkPipelineLayoutCreateInfo pl = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange,
    };
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    if (vkCreatePipelineLayout(device, &pl, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create spawn pipeline layout");
    }

    // Descriptor pool (1 set, 3 storage buffers)
    VkDescriptorPoolSize poolSize = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 3 * 2 };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    VkDescriptorSet descriptorSets[2];
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create spawn descriptor pool");
    }

    // Allocate descriptor set
    VkDescriptorSetLayout setLayouts[2] = { descriptorSetLayout, descriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = setLayouts,
    };
    if (vkAllocateDescriptorSets(device, &allocInfo, descriptorSets) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate spawn descriptor set");
    }

    // Descriptor sets info
    VkDeviceSize halfSize = (MAX_PARTICLES / 2) * sizeof(Particle);
    VkDescriptorBufferInfo outInfo[2] = {
        { .buffer = particleBuffer, .offset = 0,        .range = halfSize },
        { .buffer = particleBuffer, .offset = halfSize, .range = halfSize },
    };
    VkDescriptorBufferInfo countersInfo = {
        .buffer = countersBuffer,
        .offset = 0,
        .range  = sizeof(CountersAndDrawCmd),
    };
    VkDescriptorBufferInfo spawnInfo = { .buffer = spawnBuffer, .offset = 0, .range  = sizeof(SpawnData) };

    // Descriptor sets
    VkWriteDescriptorSet writes[2][bindingsLen] = {};
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t outIdx = 1 - i;

        writes[i][0] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],               
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &outInfo[outIdx],                       
        };
        writes[i][1] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],               
            .dstBinding = 1,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &countersInfo,
        };
        writes[i][2] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i],               
            .dstBinding = 2,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &spawnInfo,
        };

        vkUpdateDescriptorSets(device, 3, writes[i], 0, nullptr);  
    }

    return {
        .pipeline      = CreateComputePipeline(device, "shaders/comp_particle_spawn.spv", pipelineLayout),
        .layout        = pipelineLayout,
        .descriptorSet = {
            descriptorSets[0],
            descriptorSets[1],
        },
    };
}

// ---------------------------------------------------------------------------------------------
// COUNTER PIPELINE
// ---------------------------------------------------------------------------------------------

static Pipeline CreateComputeCountersipeline(VkDevice &device, VkBuffer countersBuffer, VkBuffer spawnBuffer) {
    // Descriptor set layout
    VkDescriptorSetLayoutBinding countersBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    VkDescriptorSetLayoutBinding spawnBinding = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    };
    const uint32_t bindingsLen = 2;
    VkDescriptorSetLayoutBinding bindings[bindingsLen] = { countersBinding, spawnBinding };
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    CreateDescriptorSetLayout(device, bindings, bindingsLen, descriptorSetLayout);

    // Pipeline layout: 1 set, no push constants
    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptorSetLayout,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create counters pipeline layout");
    }

    // Pool: 1 storage buffer descriptor, 1 set
    VkDescriptorPoolSize poolSize = {
        .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 2,
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create counters descriptor pool");

    // Allocate set
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSetLayout,
    };
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate counters descriptor set");
    }

    VkDescriptorBufferInfo countersInfo = {
        .buffer = countersBuffer,
        .offset = 0,
        .range  = sizeof(CountersAndDrawCmd),
    };
    VkDescriptorBufferInfo spawnInfo = {
        .buffer = spawnBuffer,
        .offset = 0,
        .range  = sizeof(SpawnData),
    };
    VkWriteDescriptorSet writes[2] = {
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &countersInfo,
    },
    {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = 1, 
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &spawnInfo,
    }
    };
    vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);

    return Pipeline {
        .pipeline      = CreateComputePipeline(device, "shaders/comp_particle_counters.spv", pipelineLayout),
        .layout        = pipelineLayout,
        .descriptorSet = {
            descriptorSet,
        },
    };
}

// ---------------------------------------------------------------------------------------------
// GRAPHICS PIPELINE
// ---------------------------------------------------------------------------------------------

static Pipeline CreateGraphicsPipeline(RendererApplication &app, RendererSwapchain &swapchain, VkBuffer particleBuffer) {
    // Binding 0 = particle buffer (storage buffer)
    VkDescriptorSetLayoutBinding particleBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    CreateDescriptorSetLayout(app.device, &particleBinding, 1, graphicsDescriptorSetLayout);

    // Pool
    VkDescriptorPoolSize poolSize = {
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        poolSize.descriptorCount = 2,
    };
    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2,
        .poolSizeCount = 1,
        .pPoolSizes = &poolSize,
    };
    VkDescriptorPool graphicsDescriptorPool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(app.device, &poolInfo, nullptr, &graphicsDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("failed to create particle graphics descriptor pool");

    // Allocate descriptor set
    VkDescriptorSetLayout setLayouts[2] = { graphicsDescriptorSetLayout, graphicsDescriptorSetLayout };
    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = graphicsDescriptorPool,
        .descriptorSetCount = 2,
        .pSetLayouts = setLayouts,
    };
    VkDescriptorSet descriptorSets[2];
    if (vkAllocateDescriptorSets(app.device, &allocInfo, descriptorSets) != VK_SUCCESS)
        throw std::runtime_error("failed to allocate particle graphics descriptor set");

    // Update descriptor set
    VkDeviceSize halfSize = (MAX_PARTICLES / 2) * sizeof(Particle); 
    VkDescriptorBufferInfo particleInfos[2] = { 
        { .buffer = particleBuffer, .offset = 0,        .range = halfSize },
        { .buffer = particleBuffer, .offset = halfSize, .range = halfSize },
    };
    VkWriteDescriptorSet writes[2]; 
    for (uint32_t i = 0; i < 2; i++) {
        writes[i] = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptorSets[i], 
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &particleInfos[i], 
        };
    }
    vkUpdateDescriptorSets(app.device, 2, writes, 0, nullptr); 

    // SHADERS
    VkShaderModule vertShaderModule = CreateShaderModule("shaders/vert_particle.spv", app.device);
    VkShaderModule fragShaderModule = CreateShaderModule("shaders/frag_particle.spv", app.device);
    VkPipelineShaderStageCreateInfo vertStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo fragStage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main",
    };
    VkPipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    // No vertex input – all from storage buffer
    VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = nullptr,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = nullptr,
    };

    // Draw points, not triangles
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo msaa = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = app.msaaSamples,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
    };

    std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
        .pDynamicStates = dynamicStates.data(),
    };

    // Push constants: mat4 viewProj + float zoom
    VkPushConstantRange pushRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(glm::mat4) + sizeof(float),
    };

    VkPipelineLayoutCreateInfo layout = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &graphicsDescriptorSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushRange,
    };

    VkPipelineLayout pipeLayout;
    if (vkCreatePipelineLayout(app.device, &layout, nullptr, &pipeLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create particle graphics pipeline layout");
    }

    // Dynamic rendering attachment info
    VkFormat colorFormat = swapchain.swapChainImageFormat;
    VkPipelineRenderingCreateInfoKHR renderInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
    };

    // Graphics pipeline
    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &msaa,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamic,
        .layout = pipeLayout,
        .renderPass = VK_NULL_HANDLE, 
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkPipeline vkPipeline;
    if (vkCreateGraphicsPipelines(app.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &vkPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create particle graphics pipeline");
    }

    vkDestroyShaderModule(app.device, vertShaderModule, nullptr);
    vkDestroyShaderModule(app.device, fragShaderModule, nullptr);

    return Pipeline {
        .pipeline      = vkPipeline,
        .layout        = pipeLayout,
        .descriptorSet = {
            descriptorSets[0],
            descriptorSets[1],
        },
    };
}

inline static ParticleSystem CreateParticleSystem(RendererApplication &app, RendererSwapchain &swapchain) {
    // Buffers
    VkBuffer particleBuffer = VK_NULL_HANDLE;
    VkBuffer spawnBuffer = VK_NULL_HANDLE;
    VkBuffer countersBuffer = VK_NULL_HANDLE;

    // Sizes
    VkDeviceSize particlesSize = sizeof(Particle) * MAX_PARTICLES;
    VkDeviceSize spawnSize = sizeof(SpawnData);
    VkDeviceSize countersSize = sizeof(CountersAndDrawCmd);

    // Memory
    VkDeviceMemory particleMemory = VK_NULL_HANDLE;
    VkDeviceMemory spawnMemory = VK_NULL_HANDLE;
    VkDeviceMemory countersMemory = VK_NULL_HANDLE;

    // Mapped
    void *spawnMapped = nullptr;
    void *countersMapped = nullptr;

    VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_2_INDIRECT_BUFFER_BIT;

    void* particlesInitial = calloc(1, particlesSize);
    if (!particlesInitial) throw std::bad_alloc();

    // BUFFERS
    CreateDeviceLocalBufferWithData(app.device, app.physicalDevice, app.commandPool, app.queue, usage, particlesInitial, particlesSize, particleBuffer, particleMemory);
    CreateBuffer(app.device, app.physicalDevice, spawnSize, usage, properties, spawnBuffer, spawnMemory);
    CreateBuffer(app.device, app.physicalDevice, countersSize, usage, properties, countersBuffer, countersMemory);

    // MEMORY MAPPINGS
    vkMapMemory(app.device, spawnMemory, 0, spawnSize, 0, &spawnMapped);
    {
        SpawnData *sd = reinterpret_cast<SpawnData *>(spawnMapped);
        sd->pos = glm::vec2(0.0f);
        sd->forward = glm::vec2(0.0f);
        sd->spawnCount = 0;
    }
    vkMapMemory(app.device, countersMemory, 0, countersSize, 0, &countersMapped);
    {
        CountersAndDrawCmd *count = reinterpret_cast<CountersAndDrawCmd *>(countersMapped);
        count->counters.inCount = 0;
        count->counters.outCount = 0;
        count->counters.maxParticles = MAX_PARTICLES;
    }

    ParticleSystem particleSystem = {
        .computeSimPipeline = CreateComputeSimPipeline(app.device, particleBuffer, countersBuffer),
        .computeSpawnPipeline = CreateComputeSpawnPipeline(app.device, particleBuffer, spawnBuffer, countersBuffer),
        .computeCountersPipeline = CreateComputeCountersipeline(app.device, countersBuffer, spawnBuffer),
        .graphicsPipeline = CreateGraphicsPipeline(app, swapchain, particleBuffer),
        .spawnMapped = spawnMapped,
        .countersMapped = countersMapped,
        .countersBuffer = countersBuffer,
    };

    // Cleanup
    free(particlesInitial);

    return particleSystem;
}