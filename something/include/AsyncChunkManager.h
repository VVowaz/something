#pragma once

#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <vector>
#include <map>
#include <memory> // Для shared_ptr
#include <glm/glm.hpp>
#include "MeshData.h" // Для MeshData

// Структура для ключа карты (аналогичная той, что в World.h или Application.h)
struct ivec2_less_acm {
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const {
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y; // y используется для Z координаты чанка
    }
};

// Прямые объявления
class World;
class Camera;
class Chunk;
class Renderer; // Нужен для Frustum Culling

// Класс, управляющий асинхронной генерацией мешей чанков
class AsyncChunkManager {
public:
    AsyncChunkManager();
    ~AsyncChunkManager(); // Должен остановить поток

    // Запрещаем копирование и присваивание
    AsyncChunkManager(const AsyncChunkManager&) = delete;
    AsyncChunkManager& operator=(const AsyncChunkManager&) = delete;

    // Запускает рабочий поток
    // Передает указатель на мир, который будет использоваться воркером
    void start(World& worldRef);

    // Сигнализирует рабочему потоку об остановке и дожидается его завершения
    void stop();

    // Вызывается каждый кадр (например, из Application::update)
    // Определяет, какие чанки нужно добавить в очередь на генерацию меша
    void updateChunkLoading(const Camera& camera, const Renderer& renderer, World& world);

    // Вызывается каждый кадр (например, из Application::run или Application::update)
    // Обрабатывает готовые меши из очереди и загружает их в GPU
    void uploadReadyMeshes(World& world);

private:
    // Указатель на объект мира (для доступа из рабочего потока)
    // Важно: Время жизни worldPtr должно быть больше или равно времени жизни AsyncChunkManager
    World* worldPtr = nullptr;

    // --- Ресурсы для управления потоком и очередями ---
    std::thread meshWorkerThread;                     // Рабочий поток
    std::queue<glm::ivec2> meshQueue;                 // Очередь координат чанков на генерацию
    std::queue<std::pair<glm::ivec2, std::shared_ptr<MeshData>>> readyMeshQueue; // Очередь готовых данных меша
    std::mutex queueMutex;                            // Мьютекс для защиты обеих очередей
    std::condition_variable conditionVar;             // Условная переменная для пробуждения потока
    std::atomic<bool> shutdownWorker = false;         // Флаг для остановки потока

    // Множество для отслеживания чанков, УЖЕ находящихся в очереди на генерацию
    // Ключ: координаты чанка, Значение: bool (просто для наличия ключа)
    std::map<glm::ivec2, bool, ivec2_less_acm> chunkInMeshQueue;
    std::mutex chunkInQueueMutex; // Мьютекс для защиты этого map

    // Рабочая функция потока (приватный метод)
    void workerLoop();
};