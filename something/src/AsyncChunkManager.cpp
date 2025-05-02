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
AsyncChunkManager::AsyncChunkManager() : shutdownWorker(false) {
    std::cout << "AsyncChunkManager created." << std::endl;
}

// Деструктор - вызывает stop() для корректного завершения потока
AsyncChunkManager::~AsyncChunkManager() {
    stop(); // Убедимся, что поток остановлен
    std::cout << "AsyncChunkManager destroyed." << std::endl;
}

// Запуск рабочего потока
void AsyncChunkManager::start(World& worldRef) {
    if (meshWorkerThread.joinable()) {
        std::cerr << "Warning: AsyncChunkManager::start called but worker thread is already running." << std::endl;
        return;
    }
    std::cout << "AsyncChunkManager: Starting mesh worker thread..." << std::endl;
    worldPtr = &worldRef; // Сохраняем указатель на мир
    shutdownWorker = false;
    meshWorkerThread = std::thread(&AsyncChunkManager::workerLoop, this); // Запускаем поток
    std::cout << "AsyncChunkManager: Mesh worker thread started." << std::endl;
}

// Остановка рабочего потока
void AsyncChunkManager::stop() {
    if (meshWorkerThread.joinable()) {
        std::cout << "AsyncChunkManager: Shutting down mesh worker thread..." << std::endl;
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            shutdownWorker = true; // Устанавливаем флаг
        }
        conditionVar.notify_all(); // Будим поток (или потоки, если их будет несколько)
        meshWorkerThread.join();   // Дожидаемся завершения
        std::cout << "AsyncChunkManager: Mesh worker thread stopped." << std::endl;
        worldPtr = nullptr; // Сбрасываем указатель на мир
    }
}

// Определение видимых чанков и постановка в очередь на мешинг
void AsyncChunkManager::updateChunkLoading(const Camera& camera, const Renderer& renderer, World& world) {
    if (!worldPtr || &world != worldPtr) {
        std::cerr << "ERROR::CHUNK_MANAGER: World pointer mismatch or null in updateChunkLoading!" << std::endl;
        return; // Защита
    }

    auto frustumPlanes = camera.getFrustumPlanes();

    // Блокируем очереди и карту отслеживания
    std::unique_lock<std::mutex> queueLock(queueMutex);
    std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);

    // Итерируем по чанкам мира
    for (auto const& [chunkCoord, chunkPtr] : world.getChunks()) {
        if (!chunkPtr) continue;



        // Проверяем видимость по фрустуму
        // Используем isAABBInFrustum из Renderer (или можно скопировать его сюда)
        if (renderer.isAABBInFrustum(chunkPtr->getAABB(), frustumPlanes)) {

            // Чанк виден. Проверяем, нужен ли меш и не в очереди ли он уже.
            if (chunkPtr->isDataLoaded && // Генерируем, только если данные блока загружены!
                (chunkPtr->needsMeshUpdate || !chunkPtr->hasMeshGPU) && // Нужна генерация или меша нет в GPU
                !chunkPtr->isGeneratingMesh && // И он не генерируется прямо сейчас
                chunkInMeshQueue.find(chunkCoord) == chunkInMeshQueue.end()) // И его нет в очереди
            {
                // Ставим в очередь на генерацию
                meshQueue.push(chunkCoord);
                chunkInMeshQueue[chunkCoord] = true; // Помечаем как добавленный в очередь

                // std::cout << "DEBUG: Queued chunk (" << chunkCoord.x << "," << chunkCoord.y << ") for meshing." << std::endl;

                // Уведомляем рабочий поток ПОСЛЕ разблокировки (или перед?)
                // Лучше после, чтобы он сразу мог захватить мьютекс
                queueLock.unlock();
                inQueueLock.unlock();
                conditionVar.notify_one(); // Будим один поток
                queueLock.lock();     // Блокируем обратно для след. итерации
                inQueueLock.lock();
            }
        }
        else {
            // Чанк не виден. Опционально: выгрузка меша.
            // chunkPtr->unloadMesh(); // Лучше делать по таймеру, чтобы избежать мерцания
        }
    }
    // Мьютексы разблокируются автоматически
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


// Рабочая функция потока мешинга
void AsyncChunkManager::workerLoop() {
    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads > 1) numThreads -= 1; 
    if (numThreads == 0) numThreads = 1;
    std::cout << "Mesh worker thread loop started." << std::endl;
    while (true) {
        glm::ivec2 chunkCoordToProcess;
        bool taskFound = false;

        // --- Ожидание Задачи ---
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            conditionVar.wait(lock, [this] {
                return !meshQueue.empty() || shutdownWorker;
                });

            if (shutdownWorker) {
                std::cout << "Mesh worker thread shutting down (flag detected)." << std::endl;
                return; // Выход
            }

            // Получаем задачу, если она есть
            if (!meshQueue.empty()) {
                chunkCoordToProcess = meshQueue.front();
                meshQueue.pop();
                taskFound = true;

                // Помечаем, что чанк в обработке (ставим флаг в самом чанке)
                if (worldPtr) { // Убедимся, что указатель на мир валиден
                    Chunk* chunk = worldPtr->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH);
                    if (chunk) {
                        bool expected = false;
                        // Пытаемся атомарно установить флаг "генерируется"
                        if (!chunk->isGeneratingMesh.compare_exchange_strong(expected, true)) {
                            // Если флаг уже был true, значит другой поток (если их >1) или
                            // предыдущая итерация уже обрабатывает - пропускаем
                            std::cout << "Mesh worker: Chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") generation already in progress, skipping." << std::endl;
                            taskFound = false; // Считаем, что задачи нет
                            // Убираем из карты отслеживания очереди, т.к. задача не взята
                            std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                            chunkInMeshQueue.erase(chunkCoordToProcess);
                        }
                        else {
                            // Успешно захватили флаг isGeneratingMesh
                            // std::cout << "DEBUG: Worker picked up chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ")" << std::endl;
                        }
                    }
                    else {
                        taskFound = false; // Чанк не найден, задачи нет
                        // Убираем из карты отслеживания очереди
                        std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                        chunkInMeshQueue.erase(chunkCoordToProcess);
                    }
                }
                else {
                    taskFound = false; // Нет мира, задачи нет
                }
            }
        } // Мьютекс queueMutex разблокируется

        // Если не удалось взять задачу (например, уже генерируется), идем на след. итерацию
        if (!taskFound) {
            continue;
        }

        // --- Генерация Меша --- (Без блокировки)
        std::shared_ptr<MeshData> generatedData = nullptr;
        Chunk* chunk = nullptr; // Нужен указатель для сброса флага isGeneratingMesh

        if (worldPtr) {
            chunk = worldPtr->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH);
            if (chunk && chunk->isDataLoaded) { // Генерируем только если данные загружены
                // std::cout << "DEBUG: Worker generating mesh data for (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ")" << std::endl;
                generatedData = chunk->generateMeshInternalData(*worldPtr);
                // Сбрасываем флаг isGeneratingMesh ПОСЛЕ генерации
                //chunk->isGeneratingMesh = false; // Делаем это здесь
            }
            else if (chunk && !chunk->isDataLoaded) {
                std::cerr << "Warning: Worker skipped mesh generation for chunk (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") - data not loaded." << std::endl;
                // chunk->isGeneratingMesh = false; // Сбрасываем флаг, т.к. генерации не было
            }
            else {
                // Чанк не найден, isGeneratingMesh не устанавливался
            }
        }

        // Сбрасываем флаг isGeneratingMesh независимо от результата генерации
        if (chunk) {
            chunk->isGeneratingMesh = false;
        }


        // --- Добавление результата в очередь готовых мешей ---
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            // Добавляем результат (даже если generatedData == nullptr, чтобы главный поток убрал из очереди ожидания)
            readyMeshQueue.push({ chunkCoordToProcess, generatedData });
            // std::cout << "DEBUG: Worker added result for (" << chunkCoordToProcess.x << "," << chunkCoordToProcess.y << ") to ready queue." << std::endl;
        }
        // Главный поток сам проверит очередь readyMeshQueue

    } // Конец while(true)
}