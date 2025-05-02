#include "InputManager.h"
#include "Engine.h" // Включаем для доступа к флагам showGrid/showDebugAxes
#include "Camera.h"      // Включаем для CameraMovement и методов камеры
#include <GLFW/glfw3.h>  // Включаем для констант GLFW (GLFW_KEY_*, GLFW_PRESS, etc.)
#include <iostream>      // Для вывода отладки F3

// --- Конструктор ---
InputManager::InputManager(GLFWwindow* glfwWin, int windowWidth, int windowHeight)
    : window(glfwWin),
    firstMouse(true), // Начинаем с true, чтобы первое событие мыши установило lastX/Y
    lastX(static_cast<double>(windowWidth) / 2.0),   // Начальная позиция - центр
    lastY(static_cast<double>(windowHeight) / 2.0),  // Начальная позиция - центр
    f3KeyPressed(false),
    activeCamera(nullptr) // Изначально камера не установлена
{
    if (!window) {
        // Обработка ошибки, если передан нулевой указатель на окно
        std::cerr << "ERROR::INPUT_MANAGER: GLFW window pointer is null!" << std::endl;
        // Можно бросить исключение или установить флаг ошибки
    }
    std::cout << "InputManager created." << std::endl;
}

// --- Обработка всего ввода ---
void InputManager::processInput(Engine& app, Camera& camera, float deltaTime) {
    if (!window) return; // Не обрабатывать ввод, если нет окна

    // Сохраняем указатель на текущую камеру для использования в handleMouseMovement
    activeCamera = &camera;

    // Опрашиваем события (это также вызывает колбэки, установленные в Application/Window)
    // glfwPollEvents(); // Этот вызов лучше оставить в главном цикле Application::run

    // Обрабатываем нажатия клавиш для движения камеры
    processKeyboardInput(app, camera, deltaTime);

    // Обрабатываем нажатие F3 для переключения отладки
    processF3Toggle(app);

    // Обработка закрытия окна по ESC
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// --- Обработка Клавиатуры ---
void InputManager::processKeyboardInput(Engine& /*app*/, Camera& camera, float deltaTime) {
    // Movement
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::RIGHT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::UP, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        camera.processKeyboard(CameraMovement::DOWN, deltaTime);
}

// --- Обработка Переключения F3 ---
void InputManager::processF3Toggle(Engine& app) {
    int f3State = glfwGetKey(window, GLFW_KEY_F3);
    if (f3State == GLFW_PRESS && !f3KeyPressed) {
        // Переключаем флаги напрямую в объекте Application
        app.showGrid = !app.showGrid;
        app.showDebugAxes = !app.showDebugAxes;
        f3KeyPressed = true;
        std::cout << "InputManager: Debug Overlay Toggled: Grid=" << (app.showGrid ? "ON" : "OFF")
            << ", Axes=" << (app.showDebugAxes ? "ON" : "OFF") << std::endl;
    }
    else if (f3State == GLFW_RELEASE) {
        f3KeyPressed = false;
    }
}

// --- Обработка Движения Мыши (Колбэк) ---
void InputManager::handleMouseMovement(double xpos, double ypos) {
    // Если камера не установлена через processInput, ничего не делаем
    if (!activeCamera) return;

    // Обработка самого первого события движения мыши
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    // Вычисляем смещение с предыдущего кадра
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos; // Инвертируем Y

    // Обновляем последние координаты для следующего кадра
    lastX = xpos;
    lastY = ypos;

    // Передаем смещение в камеру
    activeCamera->processMouseMovement(static_cast<float>(xoffset), static_cast<float>(yoffset));
}