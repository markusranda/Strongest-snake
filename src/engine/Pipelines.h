#pragma once
#include "vulkan/vulkan.h"
#include "Shadertype.h"
#include "Vertex.h"

#include <fstream>
#include <unordered_map>

struct Pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

std::array<Pipeline, static_cast<size_t>(ShaderType::COUNT)> CreateGraphicsPipelines(VkDevice device, VkRenderPass renderPass, VkDescriptorSetLayout textureSetLayout, VkSampleCountFlagBits msaaSamples);
Pipeline createGraphicsPipeline(VkDevice device, std::string vertPath, std::string fragPath, VkRenderPass renderPass, VkDescriptorSetLayout textureSetLayout, VkSampleCountFlagBits msaaSampleCount);
VkShaderModule createShaderModule(const std::vector<char> &code, VkDevice device);
std::vector<char> readFile(const std::string &filename);