#include "Window.h"
#include <stdexcept>

Window::Window(uint32_t width, uint32_t height, const char *title) : width(width), height(height)
{
    if (!glfwInit())
    {
        throw std::runtime_error("Failed to initialize GLFW!");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window!");
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
}

Window::~Window()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

void Window::framebufferResizeCallback(GLFWwindow *window, int, int)
{
    auto appWindow = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
    appWindow->framebufferResized = true;
}
