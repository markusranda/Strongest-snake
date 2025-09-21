#pragma once
#include "vulkan/vulkan.h"
#include "Vertex.h"

#include <fstream>
#include <unordered_map>

enum class ShaderType
{
    FlatColor,
    SnakeSkin,
    COUNT
};

struct Pipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

std::array<Pipeline, static_cast<size_t>(ShaderType::COUNT)> CreateGraphicsPipelines(VkDevice device, VkRenderPass renderPass);
Pipeline createGraphicsPipeline(VkDevice device, std::string vertPath, std::string fragPath, VkRenderPass renderPass);
VkShaderModule createShaderModule(const std::vector<char> &code, VkDevice device);
std::vector<char> readFile(const std::string &filename);