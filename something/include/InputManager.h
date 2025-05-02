#pragma once

// Прямые объявления, чтобы не включать тяжелые заголовки
struct GLFWwindow; // Из <GLFW/glfw3.h>
class Camera;      // Из "Camera.h"
class Engine; // Из "Application.h"

// Класс для обработки ввода пользователя (клавиатура, мышь)
class InputManager {
public:
    // Конструктор: принимает указатель на окно GLFW
    // и начальные размеры окна для инициализации позиции мыши
    InputManager(GLFWwindow* glfwWin, int windowWidth, int windowHeight);
    ~InputManager() = default; // Простой деструктор

    // Запрещаем копирование и присваивание
    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    // Обрабатывает ввод клавиатуры и мыши за текущий кадр.
    // Обновляет состояние камеры и флаги отладки в Application.
    void processInput(Engine& app, Camera& camera, float deltaTime);

    // Обрабатывает колбэк движения мыши (вызывается из Application)
    void handleMouseMovement(double xpos, double ypos);

    // --- Методы для получения состояния (если флаги хранятся здесь) ---
    // bool shouldShowGrid() const { return showGrid; }
    // bool shouldShowDebugAxes() const { return showDebugAxes; }


private:
    GLFWwindow* window; // Указатель на окно GLFW для опроса клавиш

    // Состояние мыши
    bool firstMouse;    // Флаг первого события мыши
    double lastX;       // Последняя позиция X мыши
    double lastY;       // Последняя позиция Y мыши

    // Состояние F3 для однократного срабатывания
    bool f3KeyPressed;

    // Ссылка на камеру для передачи смещения мыши (хранить не обязательно)
    Camera* activeCamera = nullptr; // Сохраняем указатель на активную камеру

    // Метод для обработки клавиатуры
    void processKeyboardInput(Engine& app, Camera& camera, float deltaTime);

    // Метод для обработки переключения F3
    void processF3Toggle(Engine& app);
};