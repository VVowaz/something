#include "Engine.h" // Сначала свой заголовок
#include "Window.h"
#include "Renderer.h"
#include "InputManager.h" // Включаем InputManager
#include "Camera.h"
#include "World.h"
#include "WorldStorage.h"
#include "WorldLoader.h" // Включаем WorldLoader
#include "AsyncChunkManager.h" // Включаем AsyncChunkManager
#include "Shader.h"
#include "Block.h" // Для WorldDataStructure
#include "Chunk.h" // Для размеров чанка и т.д.
#include "MeshData.h"

// Остальные includes
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
#include <functional>
#include <iomanip>
#include <utility>
#include <array>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <map>
#include <chrono>


// --- Конструктор ---
Engine::Engine(int width, int height, const std::string& title)
    : initialWidth(width), initialHeight(height), windowTitle(title),
    isInitialized(false),
    // Состояние ввода инициализируется в InputManager
    showGrid(false), showDebugAxes(false), // Инициализация публичных флагов
    // Асинхронные члены инициализируются по умолчанию
    shutdownWorker(false)
{
    // Инициализация указателей
    window = nullptr; renderer = nullptr; inputManager = nullptr; camera = nullptr;
    world = nullptr; worldLoader = nullptr; chunkManager = nullptr;
    blockShader = nullptr; lineShader = nullptr;
    debugAxesVAO = 0; debugAxesVBO = 0; debugAxesVertexCount = 0;
    gridVAO = 0; gridVBO = 0; gridVertexCount = 0;
}

// --- Деструктор ---
Engine::~Engine() {
    cleanup();
}

// --- Инициализация ---
bool Engine::initialize() {
    std::cout << "Engine: Initializing..." << std::endl;
    try {
        if (!setupWindowAndInput()) { std::cerr << "Engine init failed: Window/Input setup." << std::endl; return false; }
        if (!setupOpenGL()) { std::cerr << "Engine init failed: OpenGL setup." << std::endl; return false; }
        if (!loadWorld()) { std::cerr << "Engine init failed: World loading." << std::endl; return false; }
        if (!setupGraphics()) { std::cerr << "Engine init failed: Graphics setup." << std::endl; return false; }
        if (!setupCamera()) { std::cerr << "Engine init failed: Camera setup." << std::endl; return false; }
        if (!setupDebugging()) { std::cerr << "Engine init failed: Debugging setup." << std::endl; return false; }
        if (!startAsyncManagers()) { std::cerr << "Engine init failed: Async managers start." << std::endl; return false; }

    }
    catch (const std::exception& e) {
        std::cerr << "FATAL ERROR during Engine initialization: " << e.what() << std::endl;
        cleanup();
        return false;
    }

    isInitialized = true;
    std::cout << "Engine initialized successfully." << std::endl;
    return true;
}

// --- Настройка Окна и Ввода ---
bool Engine::setupWindowAndInput() {
    std::cout << "Engine: Setting up window and input..." << std::endl;
    if (!glfwInit()) { /* ... ошибка ... */ return false; }
    window = std::make_unique<Window>(initialWidth, initialHeight, windowTitle);
    if (!window || !window->create()) { /* ... ошибка ... */ glfwTerminate(); return false; }
    // Установка колбэков
    window->setFramebufferSizeCallback([this](int w, int h) { this->onFramebufferResize(w, h); });
    window->setCursorPosCallback([this](double x, double y) { this->onMouseMovement(x, y); });
    // Создание InputManager
    inputManager = std::make_unique<InputManager>(window->getGLFWwindow(), initialWidth, initialHeight);
    if (!inputManager) { /* ... ошибка ... */ return false; }
    std::cout << "Engine: Window and Input setup complete." << std::endl;
    return true;
}

// --- Настройка OpenGL ---
bool Engine::setupOpenGL() {
    std::cout << "Engine: Setting up OpenGL..." << std::endl;
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) { /* ... ошибка ... */ return false; }
    while (glGetError() != GL_NO_ERROR);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    std::cout << "Engine: OpenGL setup complete." << std::endl;
    return true;
}

// --- Загрузка/Генерация Мира ---
bool Engine::loadWorld() {
    std::cout << "Engine: Loading/Generating world..." << std::endl;
    worldLoader = std::make_unique<WorldLoader>("myworld"); // Используем имя по умолчанию
    if (!worldLoader) { /* ... ошибка ... */ return false; }
    world = worldLoader->loadOrCreateWorld(); // Загрузчик создает и заполняет мир
    if (!world) { /* ... ошибка ... */ return false; }
    std::cout << "Engine: World ready." << std::endl;
    return true;
}

// --- Настройка Графики (Шейдеры) ---
bool Engine::setupGraphics() {
    std::cout << "Engine: Setting up graphics (shaders)..." << std::endl;
    try {
        blockShader = std::make_unique<Shader>("shaders/basic.vert", "shaders/basic.frag");
        if (!blockShader || blockShader->ID == 0) throw std::runtime_error("Failed to create block shader.");
        lineShader = std::make_unique<Shader>("shaders/line.vert", "shaders/line.frag");
        if (!lineShader || lineShader->ID == 0) throw std::runtime_error("Failed to create line shader.");
    }
    catch (const std::exception& e) { /* ... ошибка ... */ return false; }
    std::cout << "Engine: Shaders loaded." << std::endl;
    return true;
}

// --- Настройка Камеры ---
bool Engine::setupCamera() {
    std::cout << "Engine: Setting up camera..." << std::endl;
    if (!world || !window) { /* ... ошибка ... */ return false; }
    float startX = world->getWidth() / 2.0f; float startY = world->getHeight() + 15.0f;
    float startZ = world->getDepth() / 2.0f + world->getDepth();
    camera = std::make_unique<Camera>(glm::vec3(startX, startY, startZ));
    if (!camera) { /* ... ошибка ... */ return false; }
    camera->Front = glm::normalize(glm::vec3(startX, world->getHeight() * 0.5f, world->getDepth() / 2.0f) - camera->Position);
    camera->setAspectRatio((float)window->getWidth() / (float)window->getHeight());
    camera->updateCameraVectors();
    // Сброс позиции мыши в InputManager
    // if(inputManager) inputManager->resetMousePosition(...); // Нужен метод reset
    std::cout << "Engine: Camera setup complete." << std::endl;
    return true;
}

// --- Настройка Отладки (Рендерер, Сетка, Оси) ---
bool Engine::setupDebugging() {
    std::cout << "Engine: Setting up debugging visuals..." << std::endl;
    renderer = std::make_unique<Renderer>();
    if (!renderer) { /* ... ошибка ... */ return false; }
    try {
        // Вызываем приватные методы для создания геометрии отладки
        setupDebugAxesData(); // <<<--- Вызов здесь
        setupGridData();      // <<<--- Вызов здесь
    }
    catch (const std::exception& e) { /* ... ошибка ... */ return false; }
    std::cout << "Engine: Debug visuals setup complete." << std::endl;
    return true;
}

// --- Запуск Асинхронных Менеджеров ---
bool Engine::startAsyncManagers() {
    std::cout << "Engine: Starting async managers..." << std::endl;
    if (!world) { /* ... ошибка ... */ return false; }
    chunkManager = std::make_unique<AsyncChunkManager>();
    if (!chunkManager) { /* ... ошибка ... */ return false; }
    chunkManager->start(*world); // Запускаем поток менеджера чанков
    std::cout << "Engine: Async managers started." << std::endl;
    return true;
}


// --- setupDebugAxesData --- (Приватный метод Engine)
void Engine::setupDebugAxesData() {
    std::cout << "Engine: Setting up debug axes data..." << std::endl; // Добавим вывод
    GLfloat lineVertices[] = {
         0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f,
         0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f,
         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f
    };
    debugAxesVertexCount = 6;
    // Удаляем старые, если есть (на случай повторной инициализации)
    if (debugAxesVBO != 0) glDeleteBuffers(1, &debugAxesVBO);
    if (debugAxesVAO != 0) glDeleteVertexArrays(1, &debugAxesVAO);
    // Генерируем новые
    glGenVertexArrays(1, &debugAxesVAO); glGenBuffers(1, &debugAxesVBO);
    glBindVertexArray(debugAxesVAO); glBindBuffer(GL_ARRAY_BUFFER, debugAxesVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat))); glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
    std::cout << "Engine: Debug axes VAO/VBO setup complete." << std::endl;
}

// --- setupGridData --- (Приватный метод Engine)
void Engine::setupGridData() {
    std::cout << "Engine: Setting up grid data..." << std::endl;
    if (!world) { /* ... предупреждение ... */ return; }
    int w = world->getWidth(); int h = world->getHeight(); int d = world->getDepth();
    std::vector<GLfloat> gridLineVertices;
    glm::vec3 gridColor = glm::vec3(0.4f, 0.4f, 0.4f);
    gridLineVertices.reserve(((w + 1) * (h + 1) + (d + 1) * (h + 1) + (w + 1) * (d + 1)) * 2 * 6); // Резервируем память

    float yo = -0.01f; // Небольшое смещение вниз, чтобы линии были чуть под блоками

    // Линии вдоль Z (параллельно X)
    for (int y = 0; y <= h; ++y) {
        for (int x = 0; x <= w; ++x) {
            gridLineVertices.insert(gridLineVertices.end(), { (float)x, (float)y + yo, 0.0f, gridColor.r, gridColor.g, gridColor.b });
            gridLineVertices.insert(gridLineVertices.end(), { (float)x, (float)y + yo, (float)d, gridColor.r, gridColor.g, gridColor.b });
        }
    }
    // Линии вдоль X (параллельно Z)
    for (int y = 0; y <= h; ++y) {
        for (int z = 0; z <= d; ++z) {
            gridLineVertices.insert(gridLineVertices.end(), { 0.0f, (float)y + yo, (float)z, gridColor.r, gridColor.g, gridColor.b });
            gridLineVertices.insert(gridLineVertices.end(), { (float)w, (float)y + yo, (float)z, gridColor.r, gridColor.g, gridColor.b });
        }
    }
    // Линии вдоль Y (вертикальные)
    for (int x = 0; x <= w; ++x) {
        for (int z = 0; z <= d; ++z) {
            gridLineVertices.insert(gridLineVertices.end(), { (float)x, 0.0f + yo, (float)z, gridColor.r, gridColor.g, gridColor.b });
            gridLineVertices.insert(gridLineVertices.end(), { (float)x, (float)h + yo, (float)z, gridColor.r, gridColor.g, gridColor.b });
        }
    }

    if (gridLineVertices.empty()) { /* ... предупреждение ... */ gridVertexCount = 0; return; }
    gridVertexCount = static_cast<GLsizei>(gridLineVertices.size() / 6);
    if (gridVBO != 0) glDeleteBuffers(1, &gridVBO); if (gridVAO != 0) glDeleteVertexArrays(1, &gridVAO);
    glGenVertexArrays(1, &gridVAO); glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO); glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridLineVertices.size() * sizeof(GLfloat), gridLineVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat))); glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindVertexArray(0);
    std::cout << "Engine: Grid VAO/VBO setup complete. Vertex count: " << gridVertexCount << std::endl;
}


// --- Главный цикл ---
void Engine::run() {
    if (!isInitialized) {
        std::cerr << "ERROR: Cannot run uninitialized application." << std::endl;
        return;
    }
    std::cout << "Engine: Starting main loop..." << std::endl;
    float lastFrameTime = static_cast<float>(glfwGetTime());
    float timeSinceLastUnloadCheck = 0.0f; const float unloadCheckInterval = 5.0f;

    while (window && !window->shouldClose()) {
        float currentFrameTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        const float maxDeltaTime = 0.1f;
        if (deltaTime <= 0.0f) deltaTime = 0.016f;
        if (deltaTime > maxDeltaTime) deltaTime = maxDeltaTime;

        // 1. Ввод (ESC здесь, остальное в InputManager)
        processInput(deltaTime);
        if (inputManager && camera) {
            // Передаем ссылку на Engine, чтобы InputManager мог менять флаги showGrid/showDebugAxes
            inputManager->processInput(*this, *camera, deltaTime);
        }

        // 2. Обновление (Координаты + Задачи на мешинг)
        update(deltaTime);

        // 3. Обработка готовых мешей
        if (chunkManager && world) {
            // processReadyMeshes должен быть реализован в AsyncChunkManager
            chunkManager->uploadReadyMeshes(*world);
        }

        // 4. Рендеринг
        render();

        // 5. Обмен буферов и события
        if (window) { window->swapBuffers(); window->pollEvents(); }
        else { break; }
    }
    std::cout << "Engine: Exiting main loop." << std::endl;
}

// --- Обработка Ввода (Только ESC) ---
void Engine::processInput(float /*deltaTime*/) {
    if (!window) return;
    if (glfwGetKey(window->getGLFWwindow(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window->getGLFWwindow(), true);
    }
    // F3 обрабатывается в InputManager::processInput
}

// --- Обновление Состояния ---
void Engine::update(float deltaTime) {
    // 1. Обновление видимости чанков и постановка в очередь мешинга
    if (chunkManager && camera && renderer && world) {
        // Передаем renderer для использования isAABBInFrustum
        chunkManager->updateChunkLoading(*camera, *renderer, *world);
    }

    // 2. Вывод координат (остается здесь для примера)
    if (camera && world) {
        glm::vec3 camPos = camera->Position;
        // ... (расчет targetBlock...) ...
        static float timeSincePrint = 0.0f; const float printInterval = 0.5f;
        timeSincePrint += deltaTime;
        if (timeSincePrint >= printInterval) { /* ... вывод координат ... */ }
    }

    // 3. Выгрузка мешей (остается здесь для примера)
    static float timeSinceLastUnloadCheck = 0.0f; const float unloadCheckInterval = 5.0f;
    timeSinceLastUnloadCheck += deltaTime;
    if (timeSinceLastUnloadCheck >= unloadCheckInterval) {
        timeSinceLastUnloadCheck = 0.0f;
        if (world && camera && renderer && chunkManager) {
            auto frustumPlanes = camera->getFrustumPlanes(); int unloaded = 0;
            for (auto& [pos, chunkPtr] : world->getChunks()) {
                if (chunkPtr && chunkPtr->getMesh() && renderer) {
                    if (!renderer->isAABBInFrustum(chunkPtr->getAABB(), frustumPlanes)) {
                        chunkPtr->unloadMesh(); unloaded++;
                    }
                }
            }
            if (unloaded > 0) std::cout << "Engine: Unloaded " << unloaded << " chunk meshes." << std::endl;
        }
    }
}

// --- Рендеринг ---
void Engine::render() {
    if (!isInitialized || !renderer || !world || !camera || !blockShader || !lineShader) return;

    renderer->prepareFrame(); // Очистка

    glm::mat4 view = camera->getViewMatrix();
    glm::mat4 projection = camera->getProjectionMatrix();
    std::array<Plane, 6> frustumPlanes = camera->getFrustumPlanes();

    // Рендеринг сетки (используем публичный флаг Engine::showGrid)
    if (showGrid && gridVAO != 0) {
        renderer->renderGrid(*lineShader, gridVAO, gridVertexCount, view, projection);
    }

    // Рендеринг мира
    renderer->renderWorld(*world, *camera, *blockShader, frustumPlanes);

    // Рендеринг осей (используем публичный флаг Engine::showDebugAxes)
    if (showDebugAxes && debugAxesVAO != 0) {
        renderer->renderDebugInfo(*camera, *lineShader, debugAxesVAO, debugAxesVertexCount);
    }
}

// --- Освобождение ресурсов ---
void Engine::cleanup() {
    std::cout << "Engine: Cleaning up..." << std::endl;
    // 1. Остановить менеджеры
    if (chunkManager) chunkManager->stop();
    // 2. Сохранить мир
    if (isInitialized && world && worldLoader) {
        if (!worldLoader->saveWorld(*world)) { /*...*/ }
    }
    // 3. Удалить OpenGL ресурсы
    std::cout << "Engine: Deleting OpenGL resources..." << std::endl;
    if (gridVBO != 0) glDeleteBuffers(1, &gridVBO); if (gridVAO != 0) glDeleteVertexArrays(1, &gridVAO);
    if (debugAxesVBO != 0) glDeleteBuffers(1, &debugAxesVBO); if (debugAxesVAO != 0) glDeleteVertexArrays(1, &debugAxesVAO);
    gridVAO = gridVBO = debugAxesVAO = debugAxesVBO = 0; // Обнуляем для надежности
    // 4. Очистить unique_ptr (порядок важен для зависимостей)
    std::cout << "Engine: Resetting managers and components..." << std::endl;
    chunkManager.reset(); // Останавливает поток перед удалением world
    world.reset();
    worldLoader.reset(); // Зависит от worldStorage
    worldStorage.reset(); // Добавлено удаление, если он был членом Engine
    lineShader.reset(); blockShader.reset(); renderer.reset(); camera.reset(); inputManager.reset();
    window.reset(); // Удаляет контекст OpenGL
    // 5. Завершить GLFW
    if (glfwGetCurrentContext() != NULL) { glfwTerminate(); std::cout << "Engine: GLFW terminated." << std::endl; }
    isInitialized = false;
    std::cout << "Engine cleanup finished." << std::endl;
}

// --- Рабочая функция потока мешинга ---
// (Остается здесь, так как управляет членами Engine: очередями и флагом)
void Engine::meshWorkerLoop() {
    std::cout << "Mesh worker thread starting loop." << std::endl;
    while (true) {
        glm::ivec2 chunkCoordToProcess;
        bool taskFound = false; // Флаг, что задача действительно взята

        // --- Ожидание Задачи ---
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            conditionVar.wait(lock, [this] { return !meshQueue.empty() || shutdownWorker; });
            if (shutdownWorker) { /* ... выход ... */ return; }
            if (!meshQueue.empty()) {
                chunkCoordToProcess = meshQueue.front(); meshQueue.pop(); taskFound = true;
                // Пометка чанка как обрабатываемого
                {
                    std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                    auto it = chunkInMeshQueue.find(chunkCoordToProcess);
                    if (it != chunkInMeshQueue.end()) {
                        Chunk* chunk = world ? world->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH) : nullptr;
                        if (chunk) {
                            bool expected = false;
                            if (!chunk->isGeneratingMesh.compare_exchange_strong(expected, true)) {
                                taskFound = false; // Задача уже выполняется
                                // std::cout << "Mesh worker: Chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") already generating, skipping." << std::endl;
                                // chunkInMeshQueue.erase(it); // Убираем из отслеживания, т.к. не взяли
                            }
                        }
                        else { taskFound = false; chunkInMeshQueue.erase(it); }
                    }
                    else { taskFound = false; /* Не должно быть */ }
                } // Конец блокировки inQueueLock
            }
        } // Конец блокировки queueLock

        if (!taskFound) continue; // Если не взяли задачу, на новую итерацию

        // --- Генерация Меша ---
        std::shared_ptr<MeshData> generatedData = nullptr;
        Chunk* chunk = nullptr; // Указатель на чанк для сброса флага
        if (world) { // Проверяем мир
            chunk = world->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH);
            if (chunk && chunk->isDataLoaded) {
                generatedData = chunk->generateMeshInternalData(*world); // Генерация
            }
            else if (chunk && !chunk->isDataLoaded) {
                // std::cerr << "Warning: Worker skipped mesh gen for chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") - data not loaded." << std::endl;
            }
            if (chunk) { // Сбрасываем флаг генерации независимо от результата
                chunk->isGeneratingMesh = false;
            }
        }

        // --- Добавление результата в очередь ---
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            readyMeshQueue.push({ chunkCoordToProcess, generatedData }); // Добавляем пару (даже если данные null)
        }
        // Главный поток сам обработает readyMeshQueue
        // и уберет из chunkInMeshQueue после uploadMeshToGPU

    } // Конец while(true)
}


// --- Обработка Готовых Мешей ---
// (Остается здесь, т.к. работает с очередями Engine)
void Engine::processReadyMeshes() {
    std::unique_lock<std::mutex> lock(queueMutex);
    while (!readyMeshQueue.empty()) {
        auto& readyPair = readyMeshQueue.front();
        glm::ivec2 chunkCoord = readyPair.first;
        std::shared_ptr<MeshData> meshData = readyPair.second;
        readyMeshQueue.pop();
        lock.unlock(); // Разблокируем как можно раньше

        if (world) {
            Chunk* chunk = world->getChunk(chunkCoord.x * Chunk::CHUNK_WIDTH, chunkCoord.y * Chunk::CHUNK_DEPTH);
            if (chunk) {
                chunk->uploadMeshToGPU(meshData); // Загружаем в GPU
                // Убираем из карты отслеживания очереди ожидания
                {
                    std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                    chunkInMeshQueue.erase(chunkCoord); // Успешно обработан
                }
            }
            else {
                // Чанк не найден, все равно убираем из отслеживания
                std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                chunkInMeshQueue.erase(chunkCoord);
            }
        }
        else {
            // Мира нет, убираем из отслеживания
            std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
            chunkInMeshQueue.erase(chunkCoord);
        }
        lock.lock(); // Блокируем снова для while
    }
}


// --- Колбэки ---
void Engine::onFramebufferResize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    if (camera) {
        camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }
    // Обновляем позицию мыши в InputManager, если он есть
    if (inputManager) {
        // Нужен метод в InputManager для сброса/установки lastX/Y
        // inputManager->resetMousePosition(static_cast<double>(width) / 2.0, static_cast<double>(height) / 2.0);
    }
    else { // Если менеджера нет, обновляем здесь (но его еще нет на этом этапе)
        // lastX = static_cast<double>(width) / 2.0;
        // lastY = static_cast<double>(height) / 2.0;
    }

    std::cout << "Engine: Window resized to " << width << "x" << height << std::endl;
}

void Engine::onMouseMovement(double xpos, double ypos) {
    if (inputManager) { // Делегируем InputManager
        inputManager->handleMouseMovement(xpos, ypos);
    }
}