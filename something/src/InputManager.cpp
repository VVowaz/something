#include "InputManager.h"
#include "Engine.h"
#include "Camera.h"
#include "World.h"   // <<<--- Включаем для вызова world->setBlockType
#include "Chunk.h"   // <<<--- Включаем для Chunk::CHUNK_HEIGHT и т.д. (если нужно)
#include "Block.h"   // <<<--- Включаем для BlockType
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>    // Для floor
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
    processKeyboardInput(camera, deltaTime);
    processMouseInput(app, camera);

    // Обрабатываем нажатие F3 для переключения отладки
    processF3Toggle(app);

    // Обработка закрытия окна по ESC
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

// --- Обработка Клавиатуры ---
void InputManager::processKeyboardInput(Camera& camera, float deltaTime) {
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

void InputManager::processMouseInput(Engine& engine, Camera& camera) {
    World* worldPtr = engine.getWorldPtr();
    if (!worldPtr) return;
    World& world = *worldPtr;

    // --- Левая кнопка (Ломание Подсвеченного) ---
    int lmbState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    if (lmbState == GLFW_PRESS && !leftMouseButtonPressed) {
        leftMouseButtonPressed = true;
        if (engine.isBlockHighlighted) {
            glm::ivec3 blockToBreak = engine.highlightedBlock;
            std::cout << "LMB Click: Breaking block at (" << blockToBreak.x << "," << blockToBreak.y << "," << blockToBreak.z << ")" << std::endl;
            world.setBlockType(blockToBreak.x, blockToBreak.y, blockToBreak.z, BlockType::Air);
        }
        else {
            // std::cout << "LMB Click: No block highlighted." << std::endl;
        }
        // Сбрасываем флаг сразу после обработки однократного нажатия
        // leftMouseButtonPressed = false; // Или оставляем до RELEASE, как было
    }
    else if (lmbState == GLFW_RELEASE) {
        leftMouseButtonPressed = false;
    }

    // --- Правая кнопка (Установка Рядом с Подсвеченным) ---
    int rmbState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    if (rmbState == GLFW_PRESS && !rightMouseButtonPressed) {
        rightMouseButtonPressed = true;

        if (engine.isBlockHighlighted) {
            glm::ivec3 targetBlock = engine.highlightedBlock; // Блок, на который смотрим

            // --- Определение грани и соседней ячейки ---
            glm::vec3 blockCenter = glm::vec3(targetBlock) + 0.5f;
            glm::vec3 viewDirection = glm::normalize(blockCenter - camera.Position);

            const glm::vec3 faceNormals[] = {
                { 0.0f,  0.0f,  1.0f}, { 0.0f,  0.0f, -1.0f}, {-1.0f,  0.0f,  0.0f},
                { 1.0f,  0.0f,  0.0f}, { 0.0f, -1.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}
            };
            // Смещения координат для каждой нормали (для получения соседа)
            const glm::ivec3 neighbourOffsets[] = {
                { 0,  0,  -1}, { 0,  0, 1}, {1,  0,  0},
                { -1,  0,  0}, { 0, 1,  0}, { 0,  -1,  0}
            };

            float maxDot = -2.0f;
            int bestFaceIndex = -1;
            for (int i = 0; i < 6; ++i) {
                float currentDot = glm::dot(viewDirection, faceNormals[i]);
                if (currentDot > maxDot) {
                    maxDot = currentDot;
                    bestFaceIndex = i;
                }
            }
            // --- Грань и смещение определены ---

            if (bestFaceIndex != -1) {
                // Вычисляем координаты ЯЧЕЙКИ, КУДА СТАВИТЬ БЛОК
                glm::ivec3 placeCoords = targetBlock + neighbourOffsets[bestFaceIndex]; // <<<--- Смещаемся на 1 блок по нормали

                std::cout << "RMB Click: Target block (" << targetBlock.x << "," << targetBlock.y << "," << targetBlock.z << "), "
                    << "Face Index: " << bestFaceIndex << ". Attempting to place Stone at ("
                    << placeCoords.x << "," << placeCoords.y << "," << placeCoords.z << ")" << std::endl;

                // Проверяем, не совпадает ли место установки с камерой
                int camBlockX = static_cast<int>(std::floor(camera.Position.x));
                int camBlockY = static_cast<int>(std::floor(camera.Position.y));
                int camBlockZ = static_cast<int>(std::floor(camera.Position.z));

                if (placeCoords.x == camBlockX && placeCoords.y == camBlockY && placeCoords.z == camBlockZ) {
                    std::cout << "Placement failed: Cannot place block inside camera." << std::endl;
                }
                else {
                    world.setBlockType(placeCoords.x, placeCoords.y, placeCoords.z, BlockType::Stone);
                }
            }
            else {
                std::cout << "Placement failed: Could not determine placement face." << std::endl;
            }

        }
        else {
            // std::cout << "RMB Click: No block highlighted." << std::endl;
        }
        // rightMouseButtonPressed = false; // Сброс флага? Или ждать RELEASE? Оставим до RELEASE
    }
    else if (rmbState == GLFW_RELEASE) {
        rightMouseButtonPressed = false;
    }
}