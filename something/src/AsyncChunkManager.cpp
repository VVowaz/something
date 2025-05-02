#include "AsyncChunkManager.h"
#include "World.h"   // Включаем для работы с World и Chunk
#include "Chunk.h"   // Включаем для работы с Chunk
#include "Camera.h"  // Включаем для получения Frustum Planes
#include "Renderer.h"// Включаем для isAABBInFrustum (или перенести метод сюда)
#include "MeshData.h"
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory> // Для std::shared_ptr

// Конструктор
// --- Конструктор ---
// Определяет количество потоков
AsyncChunkManager::AsyncChunkManager(unsigned int numThreads) : shutdownWorker(false) {
    if (numThreads == 0) {
        // Автоопределение: количество ядер CPU минус один (для главного потока)
        numWorkerThreads = std::thread::hardware_concurrency();
        if (numWorkerThreads > 1) {
            numWorkerThreads -= 1; // Оставляем одно ядро для главного потока и ОС
        }
        if (numWorkerThreads == 0) {
            numWorkerThreads = 1; // Минимум один рабочий поток
        }
    }
    else {
        numWorkerThreads = numThreads;
    }
    std::cout << "AsyncChunkManager created. Using " << numWorkerThreads << " worker thread(s)." << std::endl;
}

// --- Деструктор ---
AsyncChunkManager::~AsyncChunkManager() {
    stop();
    std::cout << "AsyncChunkManager destroyed." << std::endl;
}

// --- Запуск Потоков ---
// *** ИЗМЕНЕНО: Запускает несколько потоков ***
void AsyncChunkManager::start(World& worldRef) {
    if (!workerThreads.empty()) {
        std::cerr << "Warning: AsyncChunkManager::start called but worker threads are already running." << std::endl;
        return;
    }
    std::cout << "AsyncChunkManager: Starting " << numWorkerThreads << " mesh worker thread(s)..." << std::endl;
    worldPtr = &worldRef;
    shutdownWorker = false;
    workerThreads.reserve(numWorkerThreads); // Резервируем место в векторе
    for (unsigned int i = 0; i < numWorkerThreads; ++i) {
        // Запускаем каждый поток, передавая ему указатель на метод workerLoop
        workerThreads.emplace_back(&AsyncChunkManager::workerLoop, this);
    }
    std::cout << "AsyncChunkManager: Mesh worker threads started." << std::endl;
}

// --- Остановка Потоков ---
// *** ИЗМЕНЕНО: Останавливает все потоки ***
void AsyncChunkManager::stop() {
    if (workerThreads.empty()) {
        return; // Потоки не запущены
    }

    std::cout << "AsyncChunkManager: Shutting down " << workerThreads.size() << " mesh worker thread(s)..." << std::endl;
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        shutdownWorker = true; // Устанавливаем флаг
    }
    conditionVar.notify_all(); // <<<--- Будим ВСЕ потоки

    // Дожидаемся завершения каждого потока
    for (auto& thread : workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads.clear(); // Очищаем вектор потоков
    worldPtr = nullptr;
    std::cout << "AsyncChunkManager: Mesh worker threads stopped." << std::endl;
}

// --- Обновление Загрузки Чанков ---
// *** ИЗМЕНЕНО: Уведомляем все потоки ***
void AsyncChunkManager::updateChunkLoading(const Camera& camera, const Renderer& renderer, World& world) {
    if (!worldPtr || &world != worldPtr) { /*...*/ return; }
    auto frustumPlanes = camera.getFrustumPlanes();
    bool notified = false; // Флаг, чтобы уведомить только один раз

    std::unique_lock<std::mutex> queueLock(queueMutex);
    std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);

    for (auto const& [chunkCoord, chunkPtr] : world.getChunks()) {
        if (!chunkPtr) continue;

        if (renderer.isAABBInFrustum(chunkPtr->getAABB(), frustumPlanes)) {
            if (chunkPtr->isDataLoaded &&
                (chunkPtr->needsMeshUpdate || !chunkPtr->hasMeshGPU) &&
                !chunkPtr->isGeneratingMesh && // Проверяем флаг генерации
                chunkInMeshQueue.find(chunkCoord) == chunkInMeshQueue.end())
            {
                meshQueue.push(chunkCoord);
                chunkInMeshQueue[chunkCoord] = true;
                notified = true; // Помечаем, что нужно разбудить потоки
            }
        }
        else {
            // chunkPtr->unloadMesh(); // Выгрузка невидимых
        }
    }

    // Разблокируем мьютексы ДО уведомления
    inQueueLock.unlock();
    queueLock.unlock();

    // Если добавили хотя бы одну задачу, будим все потоки
    if (notified) {
        conditionVar.notify_all(); // <<<--- Уведомляем ВСЕ потоки
    }
}



// Загрузка готовых мешей в GPU (в главном потоке)
void AsyncChunkManager::uploadReadyMeshes(World& world) {
    if (!worldPtr || &world != worldPtr) {
        std::cerr << "ERROR::CHUNK_MANAGER: World pointer mismatch or null in uploadReadyMeshes!" << std::endl;
        return; // Защита
    }

    std::unique_lock<std::mutex> lock(queueMutex); // Блокируем очередь готовых

    while (!readyMeshQueue.empty()) {
        auto& readyPair = readyMeshQueue.front();
        glm::ivec2 chunkCoord = readyPair.first;
        std::shared_ptr<MeshData> meshData = readyPair.second;
        readyMeshQueue.pop();

        lock.unlock(); // Разблокируем очередь как можно раньше

        // Находим чанк (не-const версия)
        Chunk* chunk = world.getChunk(chunkCoord.x * Chunk::CHUNK_WIDTH, chunkCoord.y * Chunk::CHUNK_DEPTH);
        if (chunk) {
            // Загружаем данные в GPU
            chunk->uploadMeshToGPU(meshData);

            // Убираем из карты отслеживания очереди генерации
            {
                std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                chunkInMeshQueue.erase(chunkCoord);
            }
            // std::cout << "DEBUG: Uploaded mesh for chunk (" << chunkCoord.x << "," << chunkCoord.y << ")" << std::endl;
        }
        else {
            std::cerr << "Warning: Chunk (" << chunkCoord.x << "," << chunkCoord.y << ") not found for mesh upload." << std::endl;
            // Если чанка нет, все равно убираем из очереди отслеживания
            std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
            chunkInMeshQueue.erase(chunkCoord);
        }


        lock.lock(); // Блокируем снова для проверки while
    }
    // Мьютекс разблокируется
}


void AsyncChunkManager::workerLoop() {
    // Получаем ID потока для отладки (опционально)
    // std::cout << "Mesh worker thread [" << std::this_thread::get_id() << "] starting loop." << std::endl;

    while (true) {
        glm::ivec2 chunkCoordToProcess;
        Chunk* chunkToProcess = nullptr; // Указатель на чанк для обработки
        bool taskAcquired = false;       // Флаг, что поток успешно взял задачу

        // --- Ожидание и Взятие Задачи ---
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            conditionVar.wait(lock, [this] {
                return !meshQueue.empty() || shutdownWorker;
                });

            if (shutdownWorker) return; // Выход по флагу

            if (!meshQueue.empty()) {
                chunkCoordToProcess = meshQueue.front();
                meshQueue.pop();

                // Проверяем, не обрабатывается ли уже этот чанк и получаем указатель
                {
                    std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                    auto it = chunkInMeshQueue.find(chunkCoordToProcess);
                    if (it != chunkInMeshQueue.end()) { // Если он все еще в очереди ожидания
                        if (worldPtr) {
                            chunkToProcess = worldPtr->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH);
                            if (chunkToProcess) {
                                bool expected = false;
                                // Пытаемся атомарно захватить флаг isGeneratingMesh
                                if (chunkToProcess->isGeneratingMesh.compare_exchange_strong(expected, true)) {
                                    // Успешно захватили, задача наша!
                                    taskAcquired = true;
                                    // Можно УДАЛИТЬ из карты отслеживания очереди здесь,
                                    // так как мы уже "взяли" задачу и пометили чанк флагом isGeneratingMesh
                                    chunkInMeshQueue.erase(it);
                                    // std::cout << "DEBUG: Worker [" << std::this_thread::get_id() << "] picked up chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ")" << std::endl;
                                }
                                else {
                                    // Кто-то другой уже обрабатывает, возвращаем задачу в очередь? Нет, просто пропускаем.
                                    // std::cout << "DEBUG: Worker [" << std::this_thread::get_id() << "] skipped chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") - already generating." << std::endl;
                                    // Не удаляем из chunkInMeshQueue, т.к. мы не взяли задачу
                                    // Но нужно ли вернуть в meshQueue? Пока нет.
                                    chunkInMeshQueue.erase(it); // Убираем, т.к. кто-то уже делает
                                }
                            }
                            else {
                                chunkInMeshQueue.erase(it); // Чанк не найден
                            }
                        }
                        else {
                            chunkInMeshQueue.erase(it); // Мир не доступен
                        }
                    } // else { // Значит, кто-то другой уже взял и убрал из карты - игнорируем }
                } // Конец блокировки inQueueLock
            }
        } // Конец блокировки queueMutex

        // Если не удалось взять задачу, идем спать дальше
        if (!taskAcquired || !chunkToProcess) {
            continue;
        }

        // --- Генерация Меша --- (Без блокировки очередей)
        std::shared_ptr<MeshData> generatedData = nullptr;
        if (chunkToProcess->isDataLoaded) { // Генерируем, только если данные загружены
            generatedData = chunkToProcess->generateMeshInternalData(*worldPtr);
        }
        else {
            // std::cerr << "Warning: Worker skipped mesh gen for chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") - data not loaded." << std::endl;
        }

        // Сбрасываем флаг isGeneratingMesh ПОСЛЕ генерации (или пропуска)
        // Важно делать это до добавления в очередь готовых, чтобы главный поток
        // мог снова поставить в очередь, если потребуется обновление.
        chunkToProcess->isGeneratingMesh = false;


        // --- Добавление результата в очередь готовых ---
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            readyMeshQueue.push({ chunkCoordToProcess, generatedData });
            // std::cout << "DEBUG: Worker [" << std::this_thread::get_id() << "] added result for (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") to ready queue." << std::endl;
        }
        // Не нужно будить главный поток, он сам проверяет readyMeshQueue

    } // Конец while(true)
}