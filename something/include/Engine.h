#pragma once

#include <memory>
#include <string>
#include <GL/glew.h> // Для GLuint/GLsizei
#include <atomic>   // Для shutdownWorker
#include <thread>   // Для std::thread
#include <queue>    // Для std::queue
#include <mutex>    // Для std::mutex
#include <condition_variable> // Для std::condition_variable
#include <map>      // Для std::map
#include <glm/glm.hpp> // Для glm::ivec2
#include "MeshData.h" // Для MeshData

// Прямые объявления
class Window; class Renderer; class InputManager; class Camera;
class World; class WorldLoader; class AsyncChunkManager; class Shader; class WorldStorage;

// Структура для ключа карты (если не включен другой заголовок с ней)
struct ivec2_less_eng {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }
};


// Основной класс "движка", управляющий всем приложением
class Engine {
public: // <<<--- Сделаем публичным для простоты доступа менеджеров
    // Конструктор: принимает начальные параметры окна
    Engine(int width, int height, const std::string& title);
    // Деструктор: гарантирует правильную очистку
    ~Engine();

    // Запрещаем копирование и присваивание
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // Инициализирует все подсистемы
    bool initialize();
    // Запускает главный цикл приложения
    void run();

    // *** ПУБЛИЧНЫЕ ФЛАГИ для InputManager и Renderer ***
    bool showGrid = false;
    bool showDebugAxes = false;

    World* getWorldPtr() { return world.get(); }

private:
    // --- Основные Компоненты и Менеджеры ---
    std::unique_ptr<Window> window;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<InputManager> inputManager;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<World> world;
    std::unique_ptr<WorldLoader> worldLoader;
    std::unique_ptr<AsyncChunkManager> chunkManager;
    std::unique_ptr<WorldStorage> worldStorage;

    // --- Ресурсы (Шейдеры, Отладка) ---
    std::unique_ptr<Shader> blockShader; // Шейдер для чанков
    std::unique_ptr<Shader> lineShader;  // Шейдер для сетки и осей

    GLuint debugAxesVAO = 0, debugAxesVBO = 0;
    GLsizei debugAxesVertexCount = 0;
    GLuint gridVAO = 0, gridVBO = 0;
    GLsizei gridVertexCount = 0;

    // --- Состояние Движка ---
    bool isInitialized = false;
    const int initialWidth;
    const int initialHeight;
    const std::string windowTitle;

    // --- Асинхронный Мешинг ---
    // Эти члены остаются приватными, управляются через AsyncChunkManager
    std::thread meshWorkerThread;
    std::queue<glm::ivec2> meshQueue;
    std::queue<std::pair<glm::ivec2, std::shared_ptr<MeshData>>> readyMeshQueue;
    std::mutex queueMutex;
    std::condition_variable conditionVar;
    std::atomic<bool> shutdownWorker = false;
    std::map<glm::ivec2, bool, ivec2_less_eng> chunkInMeshQueue;
    std::mutex chunkInQueueMutex;


    // --- Приватные Методы Настройки ---
    bool setupWindowAndInput();
    bool setupOpenGL();
    bool loadWorld();
    bool setupGraphics();
    bool setupCamera();
    bool setupDebugging(); // <<<--- Будет вызывать setupDebugAxesData/setupGridData
    bool startAsyncManagers();

    // Приватные методы для отладки (вызываются из setupDebugging)
    void setupDebugAxesData();
    void setupGridData();

    // --- Приватные Методы Главного Цикла ---
    void processInput(float deltaTime); // Обработка ESC
    void update(float deltaTime);       // Вызов менеджеров
    void render();                      // Вызов рендерера
    void cleanup();                     // Очистка

    // Рабочая функция потока (если оставляем ее в Engine)
    // Либо ее можно перенести в AsyncChunkManager
    void meshWorkerLoop();
    // Обработка готовых мешей (если оставляем ее в Engine)
    void processReadyMeshes();

    // --- Колбэки Окна ---
    void onFramebufferResize(int width, int height);
    void onMouseMovement(double xpos, double ypos); // Делегирует InputManager

    friend class InputManager;

    glm::ivec3 highlightedBlock = glm::ivec3(0); // Инициализируем нулями
    bool isBlockHighlighted = false;             // Флаг, есть ли вообще подсвеченный блок

    void calculateTargetBlock();
};