#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera
{
    glm::vec2 position = {0.0f, 0.0f};
    float zoom = 1.0f;
    float rotation = 0.0f;
    uint32_t screenW = 0;
    uint32_t screenH = 0;

    glm::mat4 getViewProj() const
    {
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, -glm::vec3(position, 0.0f));
        view = glm::rotate(view, glm::radians(rotation), glm::vec3(0, 0, 1));

        float halfW = (float)screenW * 0.5f / zoom;
        float halfH = (float)screenH * 0.5f / zoom;
        glm::mat4 proj = glm::ortho(-halfW, halfW, -halfH, halfH);

        return proj * view;
    }
};