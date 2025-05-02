#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <functional> // Для std::function

class Window {
public:
    // Типы для колбэков событий окна
    using FramebufferSizeCallback = std::function<void(int, int)>;
    using CursorPosCallback = std::function<void(double, double)>; // Колбэк для позиции мыши

    Window(int width, int height, const std::string& title);
    ~Window();

    // Запрещаем копирование
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create();
    bool shouldClose() const;
    void swapBuffers();
    void pollEvents();

    GLFWwindow* getGLFWwindow() const { return glfwWindow; }
    int getWidth() const { return width; }
    int getHeight() const { return height; }

    // Методы для установки колбэков
    void setFramebufferSizeCallback(FramebufferSizeCallback callback);
    void setCursorPosCallback(CursorPosCallback callback); // Новый метод

private:
    GLFWwindow* glfwWindow = nullptr;
    int width;
    int height;
    std::string title;

    // Храним колбэки как члены класса
    FramebufferSizeCallback framebufferSizeCallback;
    CursorPosCallback cursorPosCallback; // Новый член для колбэка мыши

    // Статические колбэки GLFW, которые вызывают наши член-функции
    static void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos); // Новый статический колбэк
};