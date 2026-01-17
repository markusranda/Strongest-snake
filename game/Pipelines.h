#pragma once
#include "vulkan/vulkan.h"
#include "Shadertype.h"
#include "Vertex.h"
#include "Pipelines.h"
#include "InstanceData.h"
#include "PushConstants.h"
#include "RendererSwapchain.h"

#include <fstream>
#include <unordered_map>

// ====================================
// Datatstructures
// ====================================

struct Pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

// ====================================
// Helpers
// ====================================

inline static VkShaderModule CreateShaderModule(const char *filename, VkDevice device)
{
    uint32_t *outBuffer;

    // READ FILE
    std::ifstream file = std::ifstream(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open shader module");
    }

    size_t fileSize = (size_t)file.tellg();
    outBuffer = (uint32_t *)malloc(fileSize);

    file.seekg(0);
    file.read((char*)outBuffer, fileSize);
    file.close();

    // CREATE SHADER MODULE
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = outBuffer,
    };

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}

inline static VkDescriptorSetLayout CreateDescriptorSetLayout(VkDevice device, const VkDescriptorSetLayoutBinding* bindings, uint32_t bindingCount, VkDescriptorSetLayout &layout) {
    VkDescriptorSetLayoutCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = bindingCount,
        .pBindings = bindings,
    };

    if (vkCreateDescriptorSetLayout(device, &info, nullptr, &layout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout");
    }

    return layout;
}

inline static VkPipeline CreateComputePipeline(VkDevice device, const char* spirvPath, VkPipelineLayout pipelineLayout) {
    VkShaderModule shaderModule = CreateShaderModule(spirvPath, device);
    VkPipelineShaderStageCreateInfo stageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shaderModule,
        .pName = "main",
    };

    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stageInfo,
        .layout = pipelineLayout,
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, 0, &pipeline) != VK_SUCCESS) {
        vkDestroyShaderModule(device, shaderModule, 0);
        throw std::runtime_error("failed to create compute pipeline");
    }

    vkDestroyShaderModule(device, shaderModule, 0);

    return pipeline;
}

inline static Pipeline createGraphicsPipeline(VkDevice &device, const char *vertPath, const char *fragPath, VkDescriptorSetLayout &textureSetLayout, RendererSwapchain &swapchain, VkSampleCountFlagBits &msaaSamples)
{
    // --- Binding descriptions
    std::array<VkVertexInputBindingDescription, VertexBinding::BINDING_COUNT> bindingDescriptions = {};
    bindingDescriptions[VertexBinding::BINDING_VERTEX] = Vertex::getBindingDescription();
    bindingDescriptions[VertexBinding::BINDING_INSTANCE] = InstanceData::getBindingDescription();
    
    auto vertexAttrs = Vertex::getAttributeDescriptions();
    auto instanceAttrs = InstanceData::getAttributeDescriptions();
    std::array<VkVertexInputAttributeDescription, Vertex::ATTRIBUTE_COUNT + InstanceData::ATTRIBUTE_COUNT> attributeDescriptions{};
    size_t i = 0;
    for (auto &attr : vertexAttrs) attributeDescriptions[i++] = attr;
    for (auto &attr : instanceAttrs) attributeDescriptions[i++] = attr;
    
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = (uint32_t)(bindingDescriptions.size()),
        .pVertexBindingDescriptions = bindingDescriptions.data(),
        .vertexAttributeDescriptionCount = (uint32_t)(attributeDescriptions.size()),
        .pVertexAttributeDescriptions = attributeDescriptions.data(),
    };

    // --- Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = msaaSamples,
        .sampleShadingEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
    };

    VkDynamicState dynamicStates[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState = { 
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamicStates,
    };

    // --- Push constants ---
    VkPushConstantRange vertexPCRange = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(CameraPushConstant),
    };
    VkPushConstantRange fragPCRange = {
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = sizeof(CameraPushConstant),
        .size = sizeof(FragPushConstant),
    };
    std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPCRange, fragPCRange };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &textureSetLayout,
        .pushConstantRangeCount = (uint32_t)(pushConstantRanges.size()),
        .pPushConstantRanges = pushConstantRanges.data(),
    };

    VkPipelineLayout pipelineLayout;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkPipelineRenderingCreateInfo renderInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &swapchain.swapChainImageFormat,
    };

    // --- Shaders ---
    VkShaderModule vertShaderModule = CreateShaderModule(vertPath, device);
    VkShaderModule fragShaderModule = CreateShaderModule(fragPath, device);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertShaderModule,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragShaderModule,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo shaderStages[2] = { vertShaderStageInfo, fragShaderStageInfo };

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = pipelineLayout,
        .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkPipeline graphicsPipeline;
    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(device, fragShaderModule, nullptr);
    vkDestroyShaderModule(device, vertShaderModule, nullptr);

    return Pipeline{graphicsPipeline, pipelineLayout};
}

inline static std::array<Pipeline, (size_t)ShaderType::COUNT> CreateGraphicsPipelines(VkDevice &device, VkDescriptorSetLayout &textureSetLayout, RendererSwapchain &swapChain, VkSampleCountFlagBits &msaaSamples)
{
    std::array<Pipeline, (size_t)ShaderType::COUNT> pipelines;

    pipelines[(size_t)ShaderType::FlatColor] = createGraphicsPipeline(device, "shaders/vert_texture.spv", "shaders/frag_flat.spv", textureSetLayout, swapChain, msaaSamples);
    pipelines[(size_t)ShaderType::Texture] = createGraphicsPipeline(device, "shaders/vert_texture.spv", "shaders/frag_texture.spv", textureSetLayout, swapChain, msaaSamples);
    pipelines[(size_t)ShaderType::TextureScrolling] = createGraphicsPipeline(device, "shaders/vert_texture.spv", "shaders/frag_texture_scrolling.spv", textureSetLayout, swapChain, msaaSamples);
    pipelines[(size_t)ShaderType::TextureParallax] = createGraphicsPipeline(device, "shaders/vert_texture.spv", "shaders/frag_texture_parallax.spv", textureSetLayout, swapChain, msaaSamples);
    pipelines[(size_t)ShaderType::Border] = createGraphicsPipeline(device, "shaders/vert_texture.spv", "shaders/frag_border.spv", textureSetLayout, swapChain, msaaSamples);

    return pipelines;
}
