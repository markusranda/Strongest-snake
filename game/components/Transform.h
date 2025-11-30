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

    Transform(glm::vec2 position, glm::vec2 size, const char *name = "not defined")
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

    glm::vec2 getCenter()
    {
        return position + size / 2.0f;
    }

    float getRadius()
    {
        return size.x / 2;
    }

    void commit()
    {
        model = transformToModelMatrix();
    }

private:
    glm::mat4 transformToModelMatrix()
    {
        ZoneScoped;

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
