#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdint>
#include <stdexcept>

void framebufferResizeCallback(GLFWwindow *window, int, int);

struct Window
{
    uint32_t height;
    uint32_t width;
    GLFWwindow *handle;
    mutable bool framebufferResized = false;

    Window(uint32_t width, uint32_t height, const char *title) : width(width), height(height)
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW!");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
        if (!handle)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window!");
        }

        glfwSetWindowUserPointer(handle, this);
        glfwSetFramebufferSizeCallback(handle, framebufferResizeCallback);
    }

    bool shouldClose() const { return glfwWindowShouldClose(handle); }
    void pollEvents() { glfwPollEvents(); }
    void waitEvents() { glfwWaitEvents(); }
    void getFramebufferSize(int &width, int &height) const { glfwGetFramebufferSize(handle, &width, &height); }
    void resetResizedFlag() { framebufferResized = false; }
};

inline void framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto appWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    appWindow->framebufferResized = true;
}