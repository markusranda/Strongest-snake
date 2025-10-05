#pragma once
#include <glm/glm.hpp>
#include "VertexBinding.h"
#include <array>

struct InstanceData
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 color;

    static constexpr size_t ATTRIBUTE_COUNT = 5; // This always needs to match number of attributes

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = VertexBinding::BINDING_INSTANCE;
        binding.stride = sizeof(InstanceData);
        binding.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, ATTRIBUTE_COUNT> getAttributeDescriptions()
    {

        std::array<VkVertexInputAttributeDescription, ATTRIBUTE_COUNT> attrs{};

        // model
        for (uint32_t i = 0; i < 4; i++)
        {
            attrs[i].binding = VertexBinding::BINDING_INSTANCE;
            attrs[i].location = 2 + i; // offset by your vertex attribute count (e.g., pos=0, color=1, uv=2)
            attrs[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attrs[i].offset = offsetof(InstanceData, model) + sizeof(glm::vec4) * i;
        }

        // color
        attrs[4].binding = VertexBinding::BINDING_INSTANCE;
        attrs[4].location = 6;
        attrs[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[4].offset = offsetof(InstanceData, color);

        return attrs;
    }
};
