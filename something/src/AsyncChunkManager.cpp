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
#include <set>

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

// --- Обновление Загрузки/Видимости Чанков ---
void AsyncChunkManager::updateChunkLoading(const Camera& camera, const Renderer& renderer, World& world) {
    if (!worldPtr || &world != worldPtr) {
        std::cerr << "ERROR::CHUNK_MANAGER: World pointer mismatch or null in updateChunkLoading!" << std::endl;
        return;
    }

    // Параметры
    const int loadDistance = 8;
    const int unloadMargin = 2;
    const int renderDistance = 6; // Должен быть <= loadDistance

    // Текущий чанк камеры
    glm::ivec2 cameraChunkPos = world.worldToChunkCoords(
        static_cast<int>(std::floor(camera.Position.x)),
        static_cast<int>(std::floor(camera.Position.z))
    );

    // Множества для отслеживания
    std::set<glm::ivec2, ivec2_less_acm> requiredChunks; // Чанки, которые должны быть загружены
    std::vector<glm::ivec2> chunksToQueueForMeshing; // Чанки, для которых нужно сгенерировать меш

    auto frustumPlanes = camera.getFrustumPlanes(); // Получаем фрустум

    // --- Фаза 1: Определение необходимых/видимых чанков и кандидатов на мешинг ---
    // std::cout << "DEBUG: UpdateChunkLoading Phase 1 - Checking Chunks..." << std::endl;
    for (int dz = -loadDistance; dz <= loadDistance; ++dz) {
        for (int dx = -loadDistance; dx <= loadDistance; ++dx) {
            glm::ivec2 currentChunkPos = cameraChunkPos + glm::ivec2(dx, dz);

            // Проверяем, находится ли чанк в пределах мира (если мир ограничен)
            if (currentChunkPos.x < 0 || currentChunkPos.x >= world.getChunkWidth() ||
                currentChunkPos.y < 0 || currentChunkPos.y >= world.getChunkDepth()) { // Используем y для Z
                continue; // Пропускаем чанки вне мира
            }

            // Этот чанк должен быть загружен
            requiredChunks.insert(currentChunkPos);

            // --- Загрузка чанка, если он еще не загружен ---
            // loadChunk вернет указатель на существующий или только что созданный
            Chunk* chunk = world.loadChunk(currentChunkPos.x, currentChunkPos.y); // <<<--- ВЫЗОВ LOADCHUNK
            if (!chunk) {
                std::cerr << "Warning: Failed to load/create chunk at (" << currentChunkPos.x << "," << currentChunkPos.y << ")" << std::endl;
                continue; // Пропускаем, если не удалось создать/загрузить
            }

            // Проверяем, находится ли он в радиусе рендера
            if (std::abs(dx) <= renderDistance && std::abs(dz) <= renderDistance) {
                // Проверяем фрустум
                if (renderer.isAABBInFrustum(chunk->getAABB(), frustumPlanes)) {
                    // Чанк виден. Проверяем, нужен ли меш и не в очереди/генерации ли он.
                    bool needs_mesh = chunk->isDataLoaded &&
                        (chunk->needsMeshUpdate || !chunk->hasMeshGPU);
                    bool is_generating = chunk->isGeneratingMesh.load();

                    // Проверяем очередь ожидания (под своим мьютексом)
                    bool in_queue = false;
                    {
                        std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
                        in_queue = (chunkInMeshQueue.find(currentChunkPos) != chunkInMeshQueue.end());
                    }

                    // Отладочный вывод для видимых чанков
                    // std::cout << "  Chunk(" << currentChunkPos.x << "," << currentChunkPos.y << "): "
                    //           << "inFrustum=1"
                    //           << ", dataLoaded=" << chunk->isDataLoaded.load() // Читаем актуальное значение
                    //           << ", needsUpdate=" << chunk->needsMeshUpdate.load()
                    //           << ", hasGPU=" << chunk->hasMeshGPU.load()
                    //           << ", isGenerating=" << is_generating
                    //           << ", inQueueMap=" << in_queue
                    //           << " -> ShouldQueue=" << (needs_mesh && !is_generating && !in_queue) << std::endl;


                    if (needs_mesh && !is_generating && !in_queue) {
                        chunksToQueueForMeshing.push_back(currentChunkPos); // Добавляем кандидата на мешинг
                    }
                } // end if inFrustum
            } // end if in renderDistance
        } // end for dx
    } // end for dz

    // --- Фаза 2: Выгрузка ненужных чанков ---
    std::vector<glm::ivec2> loadedChunkCoords;
    // Получаем ключи ТОЛЬКО активных чанков
    for (const auto& pair : world.getActiveChunks()) {
        loadedChunkCoords.push_back(pair.first);
    }
    int unloadedCount = 0;
    for (const auto& coord : loadedChunkCoords) {
        // Если загруженный чанк НЕ входит в РАСШИРЕННЫЙ радиус загрузки
        if (requiredChunks.find(coord) == requiredChunks.end()) {
            if (std::abs(coord.x - cameraChunkPos.x) > loadDistance + unloadMargin ||
                std::abs(coord.y - cameraChunkPos.y) > loadDistance + unloadMargin) {
                world.unloadChunk(coord.x, coord.y); // Выгружаем
                unloadedCount++;
            }
        }
    }
    if (unloadedCount > 0) std::cout << "DEBUG: Unloaded " << unloadedCount << " chunks." << std::endl;


    // --- Фаза 3: Добавление в очередь и уведомление ---
    if (!chunksToQueueForMeshing.empty()) {
        bool needsNotify = false;
        {
            std::unique_lock<std::mutex> queueLock(queueMutex);
            std::unique_lock<std::mutex> inQueueLock(chunkInQueueMutex);
            // std::cout << "DEBUG: UpdateChunkLoading Phase 3 - Adding " << chunksToQueueForMeshing.size() << " chunks to mesh queue..." << std::endl;
            for (const auto& coord : chunksToQueueForMeshing) {
                // Повторно проверяем очередь и флаг генерации под мьютексом
                if (chunkInMeshQueue.find(coord) == chunkInMeshQueue.end()) {
                    Chunk* chunk = world.getChunk(coord.x * Chunk::CHUNK_WIDTH, coord.y * Chunk::CHUNK_DEPTH);
                    if (chunk && !chunk->isGeneratingMesh.load()) {
                        meshQueue.push(coord);
                        chunkInMeshQueue[coord] = true; // Помечаем как добавленный
                        needsNotify = true;
                        std::cout << "DEBUG: ACTUALLY Queued chunk (" << coord.x << "," << coord.y << ")" << std::endl;
                    }
                }
            }
        } // Мьютексы разблокируются

        if (needsNotify) {
            std::cout << "DEBUG: Notifying worker thread!" << std::endl;
            conditionVar.notify_one(); // Будим воркер
        }
    }
}


// --- Рабочая функция потока мешинга --- (Без изменений в логике генерации)
// Вызывает chunk->generateMeshInternalData(*worldPtr) для чанков из очереди
void AsyncChunkManager::workerLoop() {
    std::cout << "Mesh worker thread loop started." << std::endl;
    while (true) {
        glm::ivec2 chunkCoordToProcess;
        bool taskFound = false;

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
                        Chunk* chunk = worldPtr ? worldPtr->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH) : nullptr;
                        if (chunk) {
                            bool expected = false;
                            if (!chunk->isGeneratingMesh.compare_exchange_strong(expected, true)) {
                                taskFound = false; // Задача уже выполняется
                                // chunkInMeshQueue.erase(it); // Не убираем, пусть главный поток уберет после upload
                            }
                        }
                        else { taskFound = false; chunkInMeshQueue.erase(it); } // Чанка нет, убираем
                    }
                    else { taskFound = false; /* Не должно быть */ }
                }
            }
        } // Мьютекс queueMutex разблокируется

        if (!taskFound) continue;

        // --- Генерация Меша ---
        std::shared_ptr<MeshData> generatedData = nullptr;
        Chunk* chunk = nullptr;
        if (worldPtr) {
            chunk = worldPtr->getChunk(chunkCoordToProcess.x * Chunk::CHUNK_WIDTH, chunkCoordToProcess.y * Chunk::CHUNK_DEPTH);
            if (chunk && chunk->isDataLoaded) {
                generatedData = chunk->generateMeshInternalData(*worldPtr); // Генерация
            }
            else if (chunk && !chunk->isDataLoaded) { /* ... предупреждение ... */ }
            // Сбрасываем флаг isGeneratingMesh независимо от результата
            if (chunk) { chunk->isGeneratingMesh = false; }
        }

        // --- Добавление результата в очередь ---
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            readyMeshQueue.push({ chunkCoordToProcess, generatedData });
        }
        // Главный поток сам обработает readyMeshQueue и уберет из chunkInMeshQueue

    } // Конец while(true)
}