#include "Application.h"
#include "Window.h"
#include "Shader.h"
#include "Mesh.h" // Включаем для определения Vertex
#include "Camera.h"
#include "World.h"
#include "Block.h" // Включаем для BlockType и WorldDataStructure
#include "Renderer.h"
#include "WorldGenerator.h"
#include "WorldStorage.h" // Включаем для сохранения/загрузки

#include <GL/glew.h>
#include <GLFW/glfw3.h> // Включаем для констант клавиш и времени
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath> // Для std::floor
#include <iostream> // Для std::cout, std::cerr, std::endl
#include <vector> // Для std::vector
#include <string> // Для std::string
#include <stdexcept> // Для std::runtime_error
#include <memory> // Для std::unique_ptr, std::make_unique
#include <functional> // Для std::function
#include <iomanip> // Для std::fixed, std::setprecision
#include <utility> // Для std::move
#include <array> // Для std::array (используется в render для frustumPlanes)

Application::Application(int width, int height, const std::string& title)
    : initialWidth(width), initialHeight(height), windowTitle(title),
    isInitialized(false), firstMouse(true),
    lastX(static_cast<double>(width) / 2.0), lastY(static_cast<double>(height) / 2.0),
    showGrid(false), showDebugAxes(false), f3KeyPressed(false), // Отключены по умолчанию
    currentWorldName("myworld"), shutdownWorker(false) // Инициализируем флаг остановки
{
}

// --- Деструктор ---
Application::~Application() {
    cleanup(); // Вызываем очистку при уничтожении объекта
}

// --- Инициализация ---
bool Application::initialize() {
    std::cout << "Initializing application..." << std::endl;

    // 1. Инициализация GLFW
    if (!glfwInit()) {
        std::cerr << "ERROR: Failed to initialize GLFW" << std::endl;
        return false;
    }
    std::cout << "GLFW initialized." << std::endl;

    // 2. Создание окна
    window = std::make_unique<Window>(initialWidth, initialHeight, windowTitle);
    if (!window || !window->create()) {
        std::cerr << "ERROR: Failed to create GLFW window." << std::endl;
        glfwTerminate(); // Завершаем GLFW, если окно не создалось
        return false;
    }
    std::cout << "GLFW window created." << std::endl;

    // 3. Установка колбэков окна
    window->setFramebufferSizeCallback(
        [this](int width, int height) { this->onFramebufferResize(width, height); }
    );
    window->setCursorPosCallback(
        [this](double xpos, double ypos) { this->onMouseMovement(xpos, ypos); }
    );

    // 4. Инициализация GLEW
    glewExperimental = GL_TRUE;
    GLenum glewError = glewInit();
    if (glewError != GLEW_OK) {
        std::cerr << "ERROR: Failed to initialize GLEW: " << glewGetErrorString(glewError) << std::endl;
        cleanup(); // Очищаем ресурсы (окно, GLFW)
        return false;
    }
    // Очищаем потенциальную ошибку OpenGL, которую может сгенерировать glewInit
    while (glGetError() != GL_NO_ERROR);
    std::cout << "GLEW initialized." << std::endl;

    // 5. Начальная настройка OpenGL
    glEnable(GL_DEPTH_TEST); // Включаем тест глубины
    // glEnable(GL_CULL_FACE); // Можно раскомментировать для отсечения задних граней
    // glCullFace(GL_BACK);

    std::cout << "Core systems initialized." << std::endl;

    // --- 6. Загрузка или Генерация Мира ---
    std::cout << "Loading/Generating world..." << std::endl;
    worldStorage = std::make_unique<WorldStorage>();
    int worldWidth = 0, worldHeight = 0, worldDepth = 0;
    WorldDataStructure worldData; // Создаем ПУСТОЙ контейнер для данных
    bool worldLoadedFromFile = false;

    if (worldStorage->worldExists(currentWorldName)) {
        std::cout << "Loading existing world: " << currentWorldName << std::endl;
        // loadWorldData ЗАПОЛНЯЕТ worldData и возвращает его же
        worldData = worldStorage->loadWorldData(currentWorldName, worldWidth, worldHeight, worldDepth);
        if (worldWidth > 0 && worldHeight > 0 && worldDepth > 0 && !worldData.empty()) {
            worldLoadedFromFile = true;
            std::cout << "World loaded successfully. Dimensions: " << worldWidth << "x" << worldHeight << "x" << worldDepth << std::endl;
        }
        else {
            std::cerr << "Warning: Failed to load world data correctly from file. Generating new world." << std::endl;
            worldData.clear(); // Очищаем на случай частичной загрузки
            worldWidth = 16*20; worldHeight = 50; worldDepth = 16 * 20; // Размеры по умолчанию
        }
    }
    else {
        std::cout << "World file '" << currentWorldName << ".world' not found. Generating new world." << std::endl;
        worldWidth = 16 * 20; worldHeight = 50; worldDepth = 16 * 20; // Размеры по умолчанию
    }

    // Если мир НЕ был загружен из файла, генерируем данные
    if (!worldLoadedFromFile) {
        std::cout << "Generating new world data..." << std::endl;
        const int surfaceBaseHeight = 30;
        const int baseHeight = 28;      // Базовая высота уровня "моря"
        const float frequency = 0.05f;  // Частота волн (меньше = более пологие холмы)
        const float amplitude = 10.0f;  // Высота холмов над/под базовой высотой
        const float freqZMultiplier = 0.8f; // Немного разная частота по Z
        const float amplZMultiplier = 0.7f; // Немного разная амплитуда по Z
        WorldGenerator::SurfaceHeightFunction sineSurfaceFunc =
            [=](int x, int z) -> int {
            // Вычисляем две волны, чтобы рельеф был менее регулярным
            float waveX = std::sin(static_cast<float>(x) * frequency) * amplitude;
            float waveZ = std::cos(static_cast<float>(z) * frequency * freqZMultiplier) * amplitude * amplZMultiplier;
            // Суммируем волны и добавляем базовую высоту
            int calculatedHeight = baseHeight + static_cast<int>(waveX + waveZ);
            // Ограничиваем высоту, чтобы она не выходила за пределы мира (0 <= height < worldHeight)
            if (calculatedHeight < 0) calculatedHeight = 0;
            if (calculatedHeight >= worldHeight) calculatedHeight = worldHeight - 1; // Важно! Индекс не должен быть равен высоте
            return calculatedHeight;
            };
        // *** КОНЕЦ ИЗМЕНЕНИЯ ФУНКЦИИ ПОВЕРХНОСТИ ***
        WorldGenerator generator(worldWidth, worldHeight, worldDepth, sineSurfaceFunc);

        // ЗАПОЛНЯЕМ существующий worldData с помощью генератора
        if (!generator.fillWorldData(worldData)) { // <<<--- Используем fillWorldData
            std::cerr << "CRITICAL ERROR: World generator failed to fill data!" << std::endl;
            cleanup(); return false;
        }

        // Сохраняем сгенерированный мир
        std::cout << "Saving newly generated world..." << std::endl;
        // Создаем временный мир ТОЛЬКО для сохранения
        World tempWorldSaver(worldWidth, worldHeight, worldDepth);
        tempWorldSaver.populate(WorldDataStructure(worldData)); // Передаем КОПИЮ данных
        if (!worldStorage->saveWorld(tempWorldSaver, currentWorldName)) {
            std::cerr << "Warning: Failed to save newly generated world." << std::endl;
        }
    }

    // --- 7. Создание основного объекта World и заполнение ---
    try {
        std::cout << "Creating World object..." << std::endl;
        world = std::make_unique<World>(worldWidth, worldHeight, worldDepth);
        std::cout << "Populating World object and generating meshes..." << std::endl;
        // Передаем worldData через std::move, так как он больше не нужен здесь
        world->populate(std::move(worldData));
    }
    catch (const std::exception& e) {
        std::cerr << "CRITICAL ERROR during World creation/population: " << e.what() << std::endl;
        cleanup(); return false;
    }
    std::cout << "World object ready." << std::endl;



    // --- Настройка Остальной Сцены ---
    try {
        basicShader = std::make_unique<Shader>("shaders/basic.vert", "shaders/basic.frag");
        if (!basicShader || basicShader->ID == 0) throw std::runtime_error("Failed to create block shader.");
        lineShader = std::make_unique<Shader>("shaders/line.vert", "shaders/line.frag");
        if (!lineShader || lineShader->ID == 0) throw std::runtime_error("Failed to create line shader.");
        std::cout << "Shaders loaded." << std::endl;

        // Камера
        if (!window || !world) throw std::runtime_error("Cannot create camera.");
        float startX = world->getWidth() / 2.0f; float startY = world->getHeight() + 15.0f;
        float startZ = world->getDepth() / 2.0f + world->getDepth(); // Еще дальше
        camera = std::make_unique<Camera>(glm::vec3(startX, startY, startZ));
        camera->Front = glm::normalize(glm::vec3(startX, world->getHeight() * 0.5f, world->getDepth() / 2.0f) - camera->Position);
        camera->setAspectRatio((float)window->getWidth() / (float)window->getHeight());
        camera->updateCameraVectors();
        lastX = static_cast<double>(window->getWidth()) / 2.0; lastY = static_cast<double>(window->getHeight()) / 2.0;
        std::cout << "Camera created." << std::endl;

        // Рендерер
        renderer = std::make_unique<Renderer>();
        std::cout << "Renderer created." << std::endl;

        // Отладка
        setupDebugAxesData(); setupGridData();
        std::cout << "Debug visuals setup." << std::endl;

    }
    catch (const std::runtime_error& e) {
        std::cerr << "CRITICAL ERROR during scene setup: " << e.what() << std::endl;
        cleanup(); return false;
    }

    // *** ЗАПУСК РАБОЧЕГО ПОТОКА МЕШИНГА ***
    std::cout << "Starting mesh worker thread..." << std::endl;
    shutdownWorker = false; // Убедимся, что флаг сброшен
    meshWorkerThread = std::thread(&Application::meshWorkerLoop, this);
    std::cout << "Mesh worker thread started." << std::endl;


    isInitialized = true;
    std::cout << "Application initialized successfully." << std::endl;
    return true;
}


// --- setupScene больше не нужен, вся логика в initialize ---
// void Application::setupScene() { /* ... */ }

// --- setupDebugAxesData ---
// Настраивает VAO/VBO для отладочных осей
void Application::setupDebugAxesData() {
    GLfloat lineVertices[] = {
        // Position          Color
        0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f, // Origin Red
        1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f, // +X Red
        0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, // Origin Green
        0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f, // +Y Green (Используем ту же базу 0->1 по X для VBO)
        0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, // Origin Blue
        0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f, // +Z Blue (Используем ту же базу 0->1 по X для VBO)
        // Ось Y рисуется поворотом X на 90 вокруг Z
        // Ось Z рисуется поворотом X на -90 вокруг Y
        // НО! VBO все равно должен содержать данные для 6 вершин
        // Обновим VBO, чтобы он содержал реальные конечные точки
        // VBO Data: OriginRed, EndXRed, OriginGreen, EndYGreen, OriginBlue, EndZBlue
         0.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f, // Origin Red
         1.0f, 0.0f, 0.0f,  1.0f, 0.0f, 0.0f, // End X Red
         0.0f, 0.0f, 0.0f,  0.0f, 1.0f, 0.0f, // Origin Green
         0.0f, 1.0f, 0.0f,  0.0f, 1.0f, 0.0f, // End Y Green
         0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 1.0f, // Origin Blue
         0.0f, 0.0f, 1.0f,  0.0f, 0.0f, 1.0f  // End Z Blue
    };
    debugAxesVertexCount = 6; // 3 линии по 2 вершины
    glGenVertexArrays(1, &debugAxesVAO);
    glGenBuffers(1, &debugAxesVBO);
    glBindVertexArray(debugAxesVAO);
    glBindBuffer(GL_ARRAY_BUFFER, debugAxesVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);
    // Атрибут 0: Позиция
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    // Атрибут 1: Цвет
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

// --- setupGridData ---
// Настраивает VAO/VBO для сетки мира
void Application::setupGridData() {
    if (!world) {
        std::cerr << "Warning: Cannot setup grid data, world is null." << std::endl;
        return;
    }

    int w = world->getWidth();
    int h = world->getHeight(); // Используем полную высоту мира
    int d = world->getDepth();

    std::vector<GLfloat> gridLineVertices;
    glm::vec3 gridColor = glm::vec3(0.4f, 0.4f, 0.4f); // Цвет сетки темнее

    // Количество линий: (W+1)*(H+1) Z-линий + (D+1)*(H+1) X-линий + (W+1)*(D+1) Y-линий
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

    if (gridLineVertices.empty()) {
        std::cerr << "Warning: No grid lines generated." << std::endl;
        gridVertexCount = 0;
        return;
    }

    gridVertexCount = static_cast<GLsizei>(gridLineVertices.size() / 6);

    // Удаляем старые буферы, если они есть
    if (gridVBO != 0) glDeleteBuffers(1, &gridVBO);
    if (gridVAO != 0) glDeleteVertexArrays(1, &gridVAO);

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, gridLineVertices.size() * sizeof(GLfloat), gridLineVertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    std::cout << "Grid VAO/VBO setup complete. Vertex count: " << gridVertexCount << std::endl;
}

// --- Главный цикл ---
void Application::run() {
    if (!isInitialized) {
        std::cerr << "ERROR: Cannot run uninitialized application." << std::endl;
        return;
    }
    std::cout << "Starting main application loop..." << std::endl;
    float lastFrameTime = static_cast<float>(glfwGetTime()); // Инициализируем время
    float timeSinceLastUnloadCheck = 0.0f;
    const float unloadCheckInterval = 5.0f;

    while (!window->shouldClose()) {
        float currentFrameTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        const float maxDeltaTime = 0.1f;
        if (deltaTime <= 0.0f) deltaTime = 0.016f;
        if (deltaTime > maxDeltaTime) deltaTime = maxDeltaTime;

        processInput(deltaTime);
        update(deltaTime);       // Обновление логики + проверка видимых чанков
        processReadyMeshes();    // Обработка готовых мешей из очереди
        render();                // Рендеринг

        window->swapBuffers();
        window->pollEvents();
    }
    std::cout << "Exiting main loop." << std::endl;
}

// --- Обработка ввода ---
void Application::processInput(float deltaTime) {
    if (!window || !window->getGLFWwindow() || !camera) return;
    GLFWwindow* glfwWin = window->getGLFWwindow();

    if (glfwGetKey(glfwWin, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(glfwWin, true);
    }

    // Movement
    if (glfwGetKey(glfwWin, GLFW_KEY_W) == GLFW_PRESS) camera->processKeyboard(CameraMovement::FORWARD, deltaTime);
    if (glfwGetKey(glfwWin, GLFW_KEY_S) == GLFW_PRESS) camera->processKeyboard(CameraMovement::BACKWARD, deltaTime);
    if (glfwGetKey(glfwWin, GLFW_KEY_A) == GLFW_PRESS) camera->processKeyboard(CameraMovement::LEFT, deltaTime);
    if (glfwGetKey(glfwWin, GLFW_KEY_D) == GLFW_PRESS) camera->processKeyboard(CameraMovement::RIGHT, deltaTime);
    if (glfwGetKey(glfwWin, GLFW_KEY_SPACE) == GLFW_PRESS) camera->processKeyboard(CameraMovement::UP, deltaTime);
    if (glfwGetKey(glfwWin, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera->processKeyboard(CameraMovement::DOWN, deltaTime);

    // Toggle debug overlays
    int f3State = glfwGetKey(glfwWin, GLFW_KEY_F3);
    if (f3State == GLFW_PRESS && !f3KeyPressed) {
        showGrid = !showGrid;
        showDebugAxes = !showDebugAxes;
        f3KeyPressed = true;
        std::cout << "Debug Overlay Toggled: Grid=" << (showGrid ? "ON" : "OFF")
            << ", Axes=" << (showDebugAxes ? "ON" : "OFF") << std::endl;
    }
    else if (f3State == GLFW_RELEASE) {
        f3KeyPressed = false;
    }
}

// --- Обновление состояния ---
void Application::update(float deltaTime) {
    // Вывод координат
    if (camera && world) {
        glm::vec3 camPos = camera->Position;
        glm::vec3 targetPos = camPos + camera->Front * 2.0f; // Block in front
        int blockX = static_cast<int>(std::floor(camPos.x));
        int blockY = static_cast<int>(std::floor(camPos.y));
        int blockZ = static_cast<int>(std::floor(camPos.z));
        int targetBlockX = static_cast<int>(std::floor(targetPos.x));
        int targetBlockY = static_cast<int>(std::floor(targetPos.y));
        int targetBlockZ = static_cast<int>(std::floor(targetPos.z));

        static float timeSincePrint = 0.0f;
        const float printInterval = 0.5f;
        timeSincePrint += deltaTime;
        if (timeSincePrint >= printInterval) {
            timeSincePrint = 0.0f; // Use assignment instead of subtraction for accuracy
            std::cout << std::fixed << std::setprecision(2)
                << "Cam Pos: X=" << camPos.x << " Y=" << camPos.y << " Z=" << camPos.z
                << " | Block: (" << blockX << "," << blockY << "," << blockZ << ")"
                << " | Target: (" << targetBlockX << "," << targetBlockY << "," << targetBlockZ << ")"
                //<< " | Yaw: " << camera->Yaw << " Pitch: " << camera->Pitch
                << std::endl;
        }
    }

    // --- 2. Определение видимых чанков и постановка в очередь мешинга ---
    if (world && camera && renderer) { // Проверяем наличие всего необходимого
        auto frustumPlanes = camera->getFrustumPlanes();

        // Блокируем доступ к очереди задач и множеству отслеживания
        std::unique_lock<std::mutex> queueLock(queueMutex);
        std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);

        // Итерируем по чанкам
        for (auto const& [chunkCoord, chunkPtr] : world->getChunks()) {
            if (!chunkPtr) continue;

            // Проверяем видимость по фрустуму
            if (renderer->isAABBInFrustum(chunkPtr->getAABB(), frustumPlanes)) {
                // Чанк виден. Проверяем, нужно ли обновить/создать меш
                // и нет ли его уже в очереди на генерацию
                if ((chunkPtr->needsMeshUpdate || chunkPtr->getMesh() == nullptr) &&
                    chunkInMeshQueue.find(chunkCoord) == chunkInMeshQueue.end())
                {
                    // Добавляем координаты чанка в очередь на генерацию
                    meshQueue.push(chunkCoord);
                    // Помечаем, что чанк теперь в очереди
                    chunkInMeshQueue[chunkCoord] = true;
                    // std::cout << "DEBUG: Queued chunk (" << chunkCoord.x << "," << chunkCoord.y << ") for meshing." << std::endl;

                    // Уведомляем рабочий поток, что появилась работа
                    // Уведомление делаем ПОСЛЕ разблокировки мьютекса, чтобы поток мог сразу его захватить
                    queueLock.unlock();   // Разблокируем основной мьютекс очереди
                    inQueueLock.unlock(); // Разблокируем мьютекс отслеживания
                    conditionVar.notify_one(); // Будим поток
                    queueLock.lock();     // Блокируем обратно для следующей итерации
                    inQueueLock.lock();   // Блокируем обратно
                }
            }
            else {
                // Чанк не виден (опционально - выгрузка меша)
                // chunkPtr->unloadMesh(); // Можно делать здесь или по таймеру в run()
            }
        }
        // Мьютексы разблокируются автоматически при выходе из области видимости лока
    }
}

void Application::processReadyMeshes() {
    std::unique_lock<std::mutex> lock(queueMutex); // Блокируем очередь готовых мешей

    // Обрабатываем все меши, готовые на данный момент
    while (!readyMeshQueue.empty()) {
        // Получаем данные из очереди
        auto& readyDataPair = readyMeshQueue.front();
        glm::ivec2 chunkCoord = readyDataPair.first;
        std::shared_ptr<MeshData> meshData = readyDataPair.second;
        readyMeshQueue.pop(); // Удаляем из очереди

        // Разблокируем мьютекс как можно раньше
        lock.unlock();

        // Находим соответствующий чанк в мире
        if (world) {
            Chunk* chunk = world->getChunk(chunkCoord.x * Chunk::CHUNK_WIDTH, chunkCoord.y * Chunk::CHUNK_DEPTH); // Получаем по мировым координатам угла
            if (chunk) {
                // Вызываем метод чанка для загрузки данных в OpenGL
                chunk->uploadMeshToGPU(meshData);

                // Убираем чанк из множества отслеживания очереди генерации
                { // Новый блок для локального мьютекса
                    std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                    chunkInMeshQueue.erase(chunkCoord);
                }

            }
            else {
                std::cerr << "Warning: Chunk (" << chunkCoord.x << "," << chunkCoord.y << ") not found to upload ready mesh." << std::endl;
            }
        }

        // Блокируем мьютекс снова для проверки условия цикла while
        lock.lock();
    }
    // Мьютекс разблокируется автоматически
}

// --- Рендеринг --- (Без изменений в логике, но теперь зависит от наличия меша)
void Application::render() {
    if (!isInitialized || !renderer || !world || !camera || !basicShader || !lineShader) return;

    renderer->prepareFrame();
    std::array<Plane, 6> frustumPlanes = camera->getFrustumPlanes();

    // Grid
    if (showGrid && gridVAO != 0 && lineShader) {
        renderer->renderGrid(*lineShader, gridVAO, gridVertexCount, camera->getViewMatrix(), camera->getProjectionMatrix());
    }

    // World Chunks - Renderer сам проверит наличие меша
    if (basicShader) {
        renderer->renderWorld(*world, *camera, *basicShader, frustumPlanes);
    }

    // Debug Axes
    if (showDebugAxes && debugAxesVAO != 0 && lineShader) {
        renderer->renderDebugInfo(*camera, *lineShader, debugAxesVAO, debugAxesVertexCount);
    }
}

// --- Освобождение ресурсов ---
// *** ИЗМЕНЕНИЯ ЗДЕСЬ: Остановка рабочего потока ***
void Application::cleanup() {
    std::cout << "Cleaning up application resources..." << std::endl;

    // --- Остановка Рабочего Потока ---
    if (meshWorkerThread.joinable()) {
        std::cout << "Shutting down mesh worker thread..." << std::endl;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            shutdownWorker = true; // Устанавливаем флаг остановки
        }
        conditionVar.notify_one(); // Будим поток, чтобы он проверил флаг
        meshWorkerThread.join();   // Дожидаемся завершения потока
        std::cout << "Mesh worker thread stopped." << std::endl;
    }
    // --- Рабочий поток остановлен ---

    // Сохраняем мир
    if (isInitialized && world && worldStorage) {
        std::cout << "Saving world before exiting..." << std::endl;
        if (!worldStorage->saveWorld(*world, currentWorldName)) {
            std::cerr << "Warning: Failed to save world on exit." << std::endl;
        }
    }
    else {
        std::cout << "Skipping world save (not initialized or world/storage missing)." << std::endl;
    }

    // Удаляем OpenGL ресурсы
    if (gridVBO != 0) { glDeleteBuffers(1, &gridVBO); gridVBO = 0; }
    if (gridVAO != 0) { glDeleteVertexArrays(1, &gridVAO); gridVAO = 0; }
    if (debugAxesVBO != 0) { glDeleteBuffers(1, &debugAxesVBO); debugAxesVBO = 0; }
    if (debugAxesVAO != 0) { glDeleteVertexArrays(1, &debugAxesVAO); debugAxesVAO = 0; }

    // Удаляем объекты C++
    renderer.reset();
    basicShader.reset();
    lineShader.reset();
    cubeMesh.reset(); // Удаляем шаблонный меш
    world.reset();    // Удаляет мир и все чанки/меши в нем
    camera.reset();
    window.reset();   // Уничтожает окно и контекст OpenGL
    worldStorage.reset();

    // Завершаем GLFW
    // Проверяем isInitialized перед вызовом glfwTerminate
    // (например, если инициализация не удалась)
    if (glfwGetCurrentContext() != NULL) { // Более надежная проверка, чем isInitialized
        // glfwTerminate должен вызываться только если GLFW был успешно инициализирован
        // Проверка isInitialized тоже должна работать, если она правильно устанавливается/сбрасывается
        glfwTerminate();
        std::cout << "GLFW terminated." << std::endl;
    }
    else if (isInitialized) {
        std::cerr << "Warning: GLFW context is null, but isInitialized is true during cleanup." << std::endl;
    }
    isInitialized = false; // Сбрасываем флаг

    std::cout << "Cleanup finished." << std::endl;
}

// --- Рабочая функция потока мешинга --- (Новый метод)
void Application::meshWorkerLoop() {
    std::cout << "Mesh worker thread starting loop." << std::endl;
    while (true) {
        glm::ivec2 chunkCoordToProcess;

        // --- Ожидание задачи ---
        { // Блок для unique_lock
            std::unique_lock<std::mutex> lock(queueMutex);
            // Ждем, пока не появится задача в очереди или не придет сигнал остановки
            conditionVar.wait(lock, [this] {
                return !meshQueue.empty() || shutdownWorker;
                });

            // Проверяем флаг остановки ПОСЛЕ пробуждения
            if (shutdownWorker) {
                std::cout << "Mesh worker thread shutting down." << std::endl;
                return; // Выход из функции потока
            }

            // Если мы здесь, значит, очередь не пуста
            chunkCoordToProcess = meshQueue.front();
            meshQueue.pop();

            // Помечаем, что чанк начал обрабатываться (важно для избежания дублирования)
            {
                std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                // Можно убрать из chunkInMeshQueue здесь или после генерации меша
                // Но лучше перед началом, чтобы избежать повторной постановки в очередь
                auto it = chunkInMeshQueue.find(chunkCoordToProcess);
                if (it != chunkInMeshQueue.end()) {
                    // Помечаем, что идет генерация (флаг в самом чанке)
                    Chunk* chunk = world ? world->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH) : nullptr;
                    if (chunk) {
                        // Атомарно проверяем и устанавливаем флаг isGeneratingMesh
                        bool expected = false;
                        if (!chunk->isGeneratingMesh.compare_exchange_strong(expected, true)) {
                            // Другой поток уже начал генерацию (маловероятно с одним воркером, но для >1 важно)
                            // Просто пропускаем эту задачу
                            std::cout << "Mesh worker: Chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") already being generated, skipping." << std::endl;
                            continue; // Переходим к следующей итерации while(true)
                        }
                        // std::cout << "DEBUG: Worker started generating mesh for (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ")" << std::endl;

                    }
                    else {
                        chunkInMeshQueue.erase(it); // Убираем из очереди, если чанка нет
                        continue; // Пропускаем
                    }
                }
                else {
                    // Не должно происходить, если логика добавления верна
                    continue;
                }
            }


        } // Мьютекс queueMutex разблокируется здесь

        // --- Генерация Меша ---
        // Выполняется без блокировки основной очереди
        std::shared_ptr<MeshData> generatedData = nullptr;
        if (world) { // Проверяем, что мир все еще существует
            Chunk* chunk = world->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH);
            if (chunk) {
                // Вызываем генерацию данных меша
                generatedData = chunk->generateMeshInternalData(*world);
                // Сбрасываем флаг isGeneratingMesh ПОСЛЕ генерации
                chunk->isGeneratingMesh = false;
            }
        }

        // --- Добавление результата в очередь готовых мешей ---
        if (generatedData) { // Добавляем, только если меш не пустой
            { // Блок для unique_lock
                std::unique_lock<std::mutex> lock(queueMutex);
                readyMeshQueue.push({ chunkCoordToProcess, generatedData });
                // std::cout << "DEBUG: Worker finished mesh for (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << "), added to ready queue." << std::endl;
            } // Мьютекс разблокируется
             // Не уведомляем главный поток здесь, он сам проверяет очередь в своем цикле
        }
        else {
            // Если меш пуст, нам все равно нужно убрать его из очереди отслеживания
            // (если мы не сделали это раньше)
            std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
            chunkInMeshQueue.erase(chunkCoordToProcess);
        }


    } // Конец while(true)
}

// --- Колбэки ---
void Application::onFramebufferResize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    glViewport(0, 0, width, height);
    if (camera) {
        camera->setAspectRatio(static_cast<float>(width) / static_cast<float>(height));
    }
    // Обновляем lastX/lastY при изменении размера, чтобы избежать скачка мыши
    lastX = static_cast<double>(width) / 2.0;
    lastY = static_cast<double>(height) / 2.0;
    std::cout << "Window resized to: " << width << "x" << height << std::endl;
}

void Application::onMouseMovement(double xpos, double ypos) {
    if (!camera) return;
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    double xoffset = xpos - lastX;
    double yoffset = lastY - ypos; // Инвертировано
    lastX = xpos;
    lastY = ypos;
    camera->processMouseMovement(static_cast<float>(xoffset), static_cast<float>(yoffset));
}