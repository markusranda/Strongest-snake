#pragma once
#include "glm/glm.hpp"
#include "InstanceData.h"
#include <vector>
#include <string>

// This only supports upper case letters
inline void createTextInstances(char *text, const size_t &textLength, glm::vec2 &startPos, std::vector<InstanceData> &instances)
{
    constexpr float CLIP_SPACE_CELL_SIZE = 1.0f / 16;
    glm::vec2 cursor = startPos;

    for (size_t i = 0; i < textLength; i++)
    {
        uint8_t ch = text[i];
        if (ch > U'Z')
        {
            // Text instance does not support characters beyond 'Z' in the UTF-8 table
            continue;
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
}