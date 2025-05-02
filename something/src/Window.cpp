#include "Window.h"
#include <iostream>
#include <string>
#include <utility>

// --- Конструктор ---
Window::Window(int width, int height, const std::string& title)
    : width(width),
    height(height),
    title(title),
    glfwWindow(nullptr),
    framebufferSizeCallback(nullptr), // Инициализация колбэков
    cursorPosCallback(nullptr)
{
}

// --- Деструктор ---
Window::~Window() {
    if (glfwWindow) {
        glfwDestroyWindow(glfwWindow);
    }
    // glfwTerminate() вызывается в Application
}

// --- Создание окна ---
bool Window::create() {
    // Настройка GLFW Hints
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);

    // Создание окна
    glfwWindow = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
    if (glfwWindow == NULL) {
        std::cerr << "ERROR: Failed to create GLFW window with title \"" << title << "\"" << std::endl;
        return false;
    }

    // Делаем контекст текущим
    glfwMakeContextCurrent(glfwWindow);

    // Сохраняем указатель на объект Window в пользовательских данных GLFW
    glfwSetWindowUserPointer(glfwWindow, this);

    // --- Установка колбэков GLFW ---
    glfwSetFramebufferSizeCallback(glfwWindow, Window::glfwFramebufferSizeCallback);
    glfwSetCursorPosCallback(glfwWindow, Window::glfwCursorPosCallback); // Устанавливаем колбэк мыши

    // --- Настройка режима ввода мыши ---
    // Захватываем курсор (он станет невидимым и не сможет покинуть окно)
    // Это стандартный режим для управления камерой в 3D-играх
    glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    // Если курсор не захватывается, убедитесь, что окно активно (в фокусе)

    // glfwSwapInterval(1); // V-Sync

    return true;
}

// --- shouldClose, swapBuffers, pollEvents --- (Без изменений)
bool Window::shouldClose() const {
    return glfwWindow ? glfwWindowShouldClose(glfwWindow) : true;
}

void Window::swapBuffers() {
    if (glfwWindow) glfwSwapBuffers(glfwWindow);
}

void Window::pollEvents() {
    glfwPollEvents();
}

// --- Установка колбэков ---
void Window::setFramebufferSizeCallback(FramebufferSizeCallback callback) {
    framebufferSizeCallback = std::move(callback);
    // Немедленный вызов для начальной установки
    if (framebufferSizeCallback && glfwWindow) {
        int fbWidth, fbHeight;
        glfwGetFramebufferSize(glfwWindow, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0) {
            framebufferSizeCallback(fbWidth, fbHeight);
        }
    }
}

// *** НОВЫЙ МЕТОД ***
void Window::setCursorPosCallback(CursorPosCallback callback) {
    cursorPosCallback = std::move(callback);
    // Начальную позицию мыши обработаем в Application при первом событии
}


// --- Статические колбэки GLFW ---
void Window::glfwFramebufferSizeCallback(GLFWwindow* glfwWin, int w, int h) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(glfwWin));
    if (windowInstance) {
        windowInstance->width = w;
        windowInstance->height = h;
        if (windowInstance->framebufferSizeCallback) {
            if (w > 0 && h > 0) {
                windowInstance->framebufferSizeCallback(w, h);
            }
        }
    }
}

// *** НОВЫЙ СТАТИЧЕСКИЙ КОЛБЭК ***
void Window::glfwCursorPosCallback(GLFWwindow* glfwWin, double xpos, double ypos) {
    Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(glfwWin));
    // Вызываем сохраненный колбэк объекта Window (если он есть)
    if (windowInstance && windowInstance->cursorPosCallback) {
        windowInstance->cursorPosCallback(xpos, ypos);
    }
}

