#pragma once
#include <vulkan/vulkan.h>
#include <string>
#include <vector>

class VulkanPipeline
{
public:
    VulkanPipeline(
        VkDevice device,
        VkExtent2D swapchainExtent,
        VkRenderPass renderPass,
        const std::string &vertShaderPath,
        const std::string &fragShaderPath);
    ~VulkanPipeline();

    VkPipeline getPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getLayout() const { return pipelineLayout; }

private:
    VkDevice device = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;

    VkShaderModule createShaderModule(const std::vector<char> &code);
    std::vector<char> readFile(const std::string &filename);
};
