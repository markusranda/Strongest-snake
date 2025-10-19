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
    glm::mat4 model = glm::mat4(1.0f);

    // TODO Implement dirty flag so we know if we need to update this guy or not

    Transform(glm::vec2 position, glm::vec2 size, char *name = "not defined")
        : position(position), size(size), name(name)
    {
        model = transformToModelMatrix();
    }
    Transform(glm::vec2 position, glm::vec2 size, float rotation, char *name = "not defined")
        : position(position), size(size), rotation(rotation), name(name)
    {
        model = transformToModelMatrix();
    }
    Transform(glm::vec2 position, glm::vec2 size, glm::vec2 pivotPoint, char *name = "not defined")
        : position(position), size(size), pivotPoint(pivotPoint), name(name)
    {
        model = transformToModelMatrix();
    }

    void commit()
    {
        model = transformToModelMatrix();
    }

private:
    glm::mat4 Transform::transformToModelMatrix()
    {
        ZoneScopedN("Transform to model");

        glm::vec2 pivotOffset = this->size * this->pivotPoint;
        glm::mat4 model = glm::mat4(1.0f);                            // Identity matrix
        model = glm::translate(model, glm::vec3(position, 0.0f));     // Append transform to the new matrix
        model = glm::translate(model, glm::vec3(pivotOffset, 0.0f));  // move pivot
        model = glm::rotate(model, rotation, glm::vec3(0, 0, 1));     // rotate around pivot
        model = glm::translate(model, glm::vec3(-pivotOffset, 0.0f)); // undo pivot
        model = glm::scale(model, glm::vec3(size, 1.0f));             // finally scale quad

        return model;
    }
};
