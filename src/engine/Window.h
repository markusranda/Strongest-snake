#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdint>

class Window
{
public:
    Window(uint32_t width, uint32_t height, const char *title);
    ~Window();

    uint32_t height;
    uint32_t width;

    GLFWwindow *getHandle() const { return window; }
    bool shouldClose() const { return glfwWindowShouldClose(window); }
    void pollEvents() { glfwPollEvents(); }
    void waitEvents() { glfwWaitEvents(); }
    void getFramebufferSize(int &width, int &height) const { glfwGetFramebufferSize(window, &width, &height); }

    bool wasResized() const { return framebufferResized; }
    void resetResizedFlag() { framebufferResized = false; }

private:
    GLFWwindow *window;
    mutable bool framebufferResized = false;

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
};
