#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct Camera
{
    glm::vec2 position = {0.0f, 0.0f};
    float zoom = 1.0f;
    float rotation = 0.0f;
    uint32_t screenW;
    uint32_t screenH;

    Camera(uint32_t screenW, uint32_t screenH) : screenW(screenW), screenH(screenH) {}

    glm::mat4 getViewProj() const
    {
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, -glm::vec3(position, 0.0f));
        view = glm::rotate(view, glm::radians(rotation), glm::vec3(0, 0, 1));
        view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));

        glm::mat4 proj = glm::ortho(0.0f, (float)screenW, 0.0f, (float)screenH);
        return proj * view;
    }
};