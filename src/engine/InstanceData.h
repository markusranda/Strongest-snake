#pragma once
#include "Atlas.h"
#include "VertexBinding.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <string>
#include <algorithm>
#include <cctype>
#include <stdexcept>

constexpr glm::vec2 ATLAS_CELL_SIZE = glm::vec2(32.0f, 32.0f);
constexpr glm::vec2 FONT_ATLAS_CELL_SIZE = glm::vec2(16.0f, 32.0f);
constexpr glm::vec2 FONT_ATLAS_SIZE = glm::vec2(1456.0f, 32.0f); // Update this when expanding fonts.png
constexpr glm::vec2 ATLAS_SIZE = glm::vec2(4096.0f, 4096.0f);    // Update this when expanding atlas.png
constexpr float CLIP_SPACE_CELL_SIZE = 1.0f / 16;

struct InstanceData
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::vec4 color;
    alignas(16) glm::vec4 uvTransform; // (uOffset, vOffset, uScale, vScale)

    static constexpr size_t ATTRIBUTE_COUNT = 6; // This always needs to match number of attributes

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

        // uvTransform (vec4)
        attrs[5].binding = VertexBinding::BINDING_INSTANCE;
        attrs[5].location = 7;
        attrs[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attrs[5].offset = offsetof(InstanceData, uvTransform);

        return attrs;
    }
};

// This only supports upper case letters
inline static std::vector<InstanceData> BuildTextInstances(
    const std::string &text,
    glm::vec2 startPos)
{
    std::vector<InstanceData> instances;
    glm::vec2 cursor = startPos;

    for (uint8_t ch : text)
    {
        if (ch > U'Z')
        {
            throw std::runtime_error("Text instance does not support characters beyond 'Z' in the UTF-8 table");
        }

        // Convert char to index in your atlas
        int glyphIndex = static_cast<int>(ch);

        // Each glyph cell is 16x32, horizontally aligned in the first row
        glm::vec2 uvOffset = glm::vec2(
            (glyphIndex * FONT_ATLAS_CELL_SIZE.x) / FONT_ATLAS_SIZE.x,
            0.0f);
        glm::vec2 uvScale = glm::vec2(
            FONT_ATLAS_CELL_SIZE.x / FONT_ATLAS_SIZE.x,
            FONT_ATLAS_CELL_SIZE.y / FONT_ATLAS_SIZE.y);

        InstanceData instance{};
        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(cursor, 0.0f));
        model = glm::scale(model, glm::vec3(CLIP_SPACE_CELL_SIZE, CLIP_SPACE_CELL_SIZE, 1.0f));

        instance.model = model;
        instance.color = glm::vec4(1, 1, 1, 1); // white
        instance.uvTransform = glm::vec4(uvOffset, uvScale);

        instances.push_back(instance);

        cursor.x += CLIP_SPACE_CELL_SIZE; // advance horizontally
    }

    return instances;
}