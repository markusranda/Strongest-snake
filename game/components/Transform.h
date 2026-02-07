#pragma once
#include "../libs/glm/glm.hpp"
#include "../libs/glm/ext/matrix_transform.hpp"

// PROFILING
#ifdef _DEBUG
#include "tracy/Tracy.hpp"
#endif

struct Transform
{
    glm::vec2 position;
    glm::vec2 size;
    glm::vec2 dir = glm::vec2(0.0f);
    float rotation = 0.0f;
    glm::vec2 pivotPoint = {0.5f, 0.5f};
    const char *name = "not defined";
    glm::mat4 model = glm::mat4(1.0f);

    void commit() {
        model = transformToModelMatrix();
    }

    glm::vec2 getCenter() {
        return position + size / 2.0f;
    }

    float getRadius() {
        return size.x / 2;
    }

private:
    // Very slow method
    glm::mat4 transformToModelMatrix()
    {
        #ifdef _DEBUG
        ZoneScoped;
        #endif

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
