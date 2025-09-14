#pragma once
#include <glm/glm.hpp>
#include <array>

struct Vertex
{
    glm::vec2 pos;   // clip-space position
    glm::vec3 color; // RGB

    static VkVertexInputBindingDescription getBindingDescription()
    {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;                             // index of the binding
        binding.stride = sizeof(Vertex);                 // how far to move to the next vertex
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // per-vertex data (not per-instance)
        return binding;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        std::array<VkVertexInputAttributeDescription, 2> attrs{};

        // position (vec2 -> 2 floats)
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32_SFLOAT; // matches vec2
        attrs[0].offset = offsetof(Vertex, pos);

        // color (vec3 -> 3 floats)
        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; // matches vec3
        attrs[1].offset = offsetof(Vertex, color);

        return attrs;
    }
};
