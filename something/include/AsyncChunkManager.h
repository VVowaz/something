#pragma once

#include <thread>
#include <vector> // <<<--- Для std::vector<std::thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include "MeshData.h"

struct ivec2_less_acm {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const { // <<<--- ПРОВЕРЬТЕ НАЛИЧИЕ 'const' ЗДЕСЬ
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y;
    }
};
class World; class Camera; class Chunk; class Renderer;

class AsyncChunkManager {
public:
    // Конструктор: принимает желаемое количество потоков
    // (0 - автоопределение по ядрам CPU)
    AsyncChunkManager(unsigned int numThreads = 0);
    ~AsyncChunkManager();

    AsyncChunkManager(const AsyncChunkManager&) = delete;
    AsyncChunkManager& operator=(const AsyncChunkManager&) = delete;

    // Запускает рабочие потоки
    void start(World& worldRef);
    // Останавливает все рабочие потоки
    void stop();

    void updateChunkLoading(const Camera& camera, const Renderer& renderer, World& world);
    void uploadReadyMeshes(World& world);

private:
    World* worldPtr = nullptr;

    // --- Ресурсы для многопоточности ---
    std::vector<std::thread> workerThreads;       // <<<--- Пул рабочих потоков
    unsigned int numWorkerThreads = 1;            // <<<--- Количество потоков
    // Очереди и синхронизация (без изменений)
    std::queue<glm::ivec2> meshQueue;
    std::queue<std::pair<glm::ivec2, std::shared_ptr<MeshData>>> readyMeshQueue;
    std::mutex queueMutex;
    std::condition_variable conditionVar;
    std::atomic<bool> shutdownWorker = false;
    std::map<glm::ivec2, bool, ivec2_less_acm> chunkInMeshQueue;
    std::mutex chunkInQueueMutex;

    // Рабочая функция (выполняется каждым потоком)
    void workerLoop();
};