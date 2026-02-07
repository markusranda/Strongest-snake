#pragma once
#include "../libs/glm/glm.hpp"
#include <array>
#include "vulkan/vulkan.h"
#include "VertexBinding.h"

struct Vertex
{
    glm::vec2 pos; // clip-space position
    glm::vec2 uv;
    static constexpr size_t ATTRIBUTE_COUNT = 2; // This always needs to match number of attributes

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = VertexBinding::BINDING_VERTEX;
        binding.stride = sizeof(Vertex);                 // how far to move to the next vertex
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // per-vertex data (not per-instance)
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, ATTRIBUTE_COUNT> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, ATTRIBUTE_COUNT> attrs{};

        // position (vec2 -> 2 floats)
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32_SFLOAT; // matches vec2
        attrs[0].offset = offsetof(Vertex, pos);

        // uv
        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32_SFLOAT;
        attrs[1].offset = offsetof(Vertex, uv);

        return attrs;
    }
};
