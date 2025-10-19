#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// PROFILING
#include "tracy/Tracy.hpp"

struct Transform
{
    glm::vec2 position;
    glm::vec2 size;
    glm::vec2 dir = glm::vec2(0.0f);
    float rotation = 0.0f;
    glm::vec2 pivotPoint = {0.5f, 0.5f};
    const char *name = "not defined";

    // TODO Implement dirty flag so we know if we need to update this guy or not

    Transform(glm::vec2 position, glm::vec2 size, char *name = "not defined") : position(position), size(size), name(name) {}
    Transform(glm::vec2 position, glm::vec2 size, float rotation, char *name = "not defined") : position(position), size(size), rotation(rotation), name(name) {}
    Transform(glm::vec2 position, glm::vec2 size, glm::vec2 pivotPoint, char *name = "not defined") : position(position), size(size), pivotPoint(pivotPoint), name(name) {}

    glm::mat4 Transform::transformToModelMatrix()
    {
        ZoneScopedN("Transform to model");

        // --- sanity checks ---
        if (size.x <= 0 || size.y <= 0)
        {
            Logger::err("Invalid Transform size: " +
                        std::to_string(size.x) + ", " + std::to_string(size.y));
            Logger::err("Position: " +
                        std::to_string(position.x) + ", " + std::to_string(position.y));
            Logger::err(std::string("Name: ") + (name ? name : "null"));
            assert(false && "Transform.size must be positive!");
        }

        // DEBUG PURPOSE ONLY
        // assert(!std::isnan(position.x) && !std::isnan(position.y) && "Transform.position contains NaN!");
        // assert(!std::isinf(position.x) && !std::isinf(position.y) && "Transform.position contains Inf!");
        // assert(!std::isnan(rotation) && !std::isinf(rotation) && "Transform.rotation invalid!");
        // assert(pivotPoint.x >= 0 && pivotPoint.x <= 1 && pivotPoint.y >= 0 && pivotPoint.y <= 1 && "Transform.pivotPoint out of range!");

        // TODO Move model calculation to the vertex shader.

        // --- calc ---
        glm::vec2 pivotOffset = this->size * this->pivotPoint;
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(position, 0.0f));     // world position
        model = glm::translate(model, glm::vec3(pivotOffset, 0.0f));  // move pivot
        model = glm::rotate(model, rotation, glm::vec3(0, 0, 1));     // rotate around pivot
        model = glm::translate(model, glm::vec3(-pivotOffset, 0.0f)); // undo pivot
        model = glm::scale(model, glm::vec3(size, 1.0f));             // finally scale quad

        return model;
    }
};
