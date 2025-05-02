#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future> // На всякий случай, хотя async пока не используем
#include <glm/glm.hpp> // Для glm::ivec2
#include <map>      // Для std::map
#include "MeshData.h" // Включаем новую структуру

// Прямые объявления
class Window;
class Shader;
class Mesh;
class Camera;
class World;
class Renderer;
class WorldStorage;

// Структура для ключа карты (если World.h не включен напрямую здесь)
struct ivec2_less_app {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }
};

class Application {
public:
    Application(int width, int height, const std::string& title);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    bool initialize();
    void run();
    std::unique_ptr<WorldStorage> worldStorage; // <<<--- Добавляем хранилище
    std::string currentWorldName = "myworld";   // <<<--- Имя мира

private:
    std::unique_ptr<Window> window;
    std::unique_ptr<Shader> basicShader;
    std::unique_ptr<Shader> lineShader;
    std::unique_ptr<Mesh> cubeMesh;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<World> world;
    std::unique_ptr<Renderer> renderer;

    bool isInitialized = false;
    const int initialWidth;
    const int initialHeight;
    const std::string windowTitle;

    // Состояние ввода мыши
    bool firstMouse = true;
    double lastX = 0.0;
    double lastY = 0.0;

    // Ресурсы для отрисовки отладочных осей
    GLuint debugAxesVAO = 0;
    GLuint debugAxesVBO = 0;
    GLsizei debugAxesVertexCount = 0;

    // Ресурсы для отрисовки сетки мира
    GLuint gridVAO = 0;
    GLuint gridVBO = 0;
    GLsizei gridVertexCount = 0;

    // *** НОВЫЕ ФЛАГИ И СОСТОЯНИЕ КЛАВИШИ F3 ***
    bool showGrid = true;        // Показывать ли сетку мира
    bool showDebugAxes = true;   // Показывать ли отладочные оси
    bool f3KeyPressed = false;   // Флаг, чтобы F3 срабатывала один раз за нажатие

    // Приватные методы
    void setupScene();
    void setupDebugAxesData();
    void setupGridData();
    void processInput(float deltaTime);
    void update(float deltaTime);
    void render();
    void cleanup();

    // Колбэки
    void onFramebufferResize(int width, int height);
    void onMouseMovement(double xpos, double ypos);

    std::thread meshWorkerThread;                     // Рабочий поток
    std::queue<glm::ivec2> meshQueue;                 // Очередь координат чанков (ChunkX, ChunkZ) на генерацию
    std::queue<std::pair<glm::ivec2, std::shared_ptr<MeshData>>> readyMeshQueue; // Очередь готовых данных меша <КоординатыЧанка, ДанныеМеша>
    std::mutex queueMutex;                            // Мьютекс для защиты ОБЕИХ очередей
    std::condition_variable conditionVar;             // Условная переменная для пробуждения рабочего потока
    std::atomic<bool> shutdownWorker = false;         // Атомарный флаг для безопасной остановки потока

    // Множество (map) для отслеживания чанков, которые УЖЕ добавлены в meshQueue,
    // чтобы не добавлять их повторно. Ключ - координаты чанка, значение - просто true.
    std::map<glm::ivec2, bool, ivec2_less_app> chunkInMeshQueue;
    std::mutex chunkInQueueMutex;                     // Отдельный мьютекс для защиты chunkInMeshQueue

    void meshWorkerLoop();

    // Обработка готовых мешей (будет вызываться в главном потоке)
    void processReadyMeshes();

};