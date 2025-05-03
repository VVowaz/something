#include "World.h"   // Включаем свой заголовок
#include "Chunk.h"   // <<<--- ВКЛЮЧАЕМ ПОЛНОЕ ОПРЕДЕЛЕНИЕ CHUNK ЗДЕСЬ
#include <cmath>
#include <iostream>
#include <utility>
#include <stdexcept> // Для runtime_error

World::World(int sizeX, int sizeY, int sizeZ,
    SurfaceHeightFunction genFunc,
    DeltaStorage&& loadedDelta)
    : worldSizeX(sizeX), worldSizeY(sizeY), worldSizeZ(sizeZ),
    generationFunction(genFunc), // Сохраняем функцию генерации
    deltaStorage(std::move(loadedDelta)) // Перемещаем загруженную дельту
{
    // Проверка размеров
    if (worldSizeX <= 0 || worldSizeY <= 0 || worldSizeZ <= 0 ||
        worldSizeX % Chunk::CHUNK_WIDTH != 0 ||
        worldSizeY != Chunk::CHUNK_HEIGHT ||
        worldSizeZ % Chunk::CHUNK_DEPTH != 0)
    {
        throw std::runtime_error("World Constructor: Invalid world dimensions or mismatch with chunk dimensions.");
    }
    // Проверка функции генерации
    if (!generationFunction) {
        throw std::runtime_error("World Constructor: Generation function is null.");
    }

    // Рассчитываем размеры в чанках
    chunksX = worldSizeX / Chunk::CHUNK_WIDTH;
    chunksZ = worldSizeZ / Chunk::CHUNK_DEPTH;

    std::cout << "World object created. Dimensions (Blocks): " << worldSizeX << "x" << worldSizeY << "x" << worldSizeZ
        << ". Dimensions (Chunks): " << chunksX << "x" << chunksZ
        << ". Initial Delta Size: " << deltaStorage.getChangeCount() << "." << std::endl;

    // НЕ создаем чанки здесь, они будут загружаться по требованию
}


// Деструктор
World::~World() {
    std::cout << "World destroyed." << std::endl;
}

void World::createChunks() {
    std::cout << "World: Creating " << chunksX * chunksZ << " chunks..." << std::endl;
    for (int cz = 0; cz < chunksZ; ++cz) {
        for (int cx = 0; cx < chunksX; ++cx) {
            glm::ivec2 chunkPos(cx, cz);
            // Передаем cx и cz в конструктор Chunk
            chunks[chunkPos] = std::make_unique<Chunk>(cx, cz); // <<<--- Убедитесь, что здесь cx, cz
        }
    }
    std::cout << "World: Chunks created." << std::endl;
}

void World::unloadAllMeshes() {
    std::cout << "World: Unloading all chunk meshes..." << std::endl;
    int unloadedCount = 0;
    for (auto const& [pos, chunkPtr] : chunks) {
        if (chunkPtr) {
            chunkPtr->unloadMesh();
            unloadedCount++;
        }
    }
    std::cout << "World: Unloaded " << unloadedCount << " chunk meshes." << std::endl;
}

// Конвертеры координат
glm::ivec2 World::worldToChunkCoords(int worldX, int worldZ) const {
    int cx = static_cast<int>(std::floor(static_cast<float>(worldX) / Chunk::CHUNK_WIDTH));
    int cz = static_cast<int>(std::floor(static_cast<float>(worldZ) / Chunk::CHUNK_DEPTH));
    return glm::ivec2(cx, cz);
}
glm::ivec3 World::worldToLocalCoords(int worldX, int worldY, int worldZ) const {
    int localX = worldX - static_cast<int>(std::floor(static_cast<float>(worldX) / Chunk::CHUNK_WIDTH)) * Chunk::CHUNK_WIDTH;
    int localY = worldY;
    int localZ = worldZ - static_cast<int>(std::floor(static_cast<float>(worldZ) / Chunk::CHUNK_DEPTH)) * Chunk::CHUNK_DEPTH;
    return glm::ivec3(localX, localY, localZ);
}

// getChunk
const Chunk* World::getChunk(int worldX, int worldZ) const {
    glm::ivec2 chunkPos = worldToChunkCoords(worldX, worldZ);
    auto it = chunks.find(chunkPos);
    return (it != chunks.end()) ? it->second.get() : nullptr;
}
Chunk* World::getChunk(int worldX, int worldZ) {
    glm::ivec2 chunkPos = worldToChunkCoords(worldX, worldZ);
    auto it = chunks.find(chunkPos);
    return (it != chunks.end()) ? it->second.get() : nullptr;
}

// --- getBaseBlockType --- (Вспомогательный, остается как есть)
BlockType World::getBaseBlockType(int worldX, int worldY, int worldZ) const {
    if (worldX < 0 || worldX >= worldSizeX ||
        worldY < 0 || worldY >= worldSizeY ||
        worldZ < 0 || worldZ >= worldSizeZ) {
        return BlockType::Air;
    }
    // Вызов функции генерации
    int surfaceY = generationFunction(worldX, worldZ);
    if (surfaceY < 0) surfaceY = 0; if (surfaceY >= worldSizeY) surfaceY = worldSizeY - 1;
    if (worldY < surfaceY) return BlockType::Stone;
    if (worldY == surfaceY) return BlockType::Grass;
    return BlockType::Air;
}


// --- Получение типа блока (Дельта -> Генератор) ---
// *** ИЗМЕНЕНО: Не использует Chunk::getBlock ***
BlockType World::getBlockType(int worldX, int worldY, int worldZ) const {
    // 1. Проверяем дельту изменений
    glm::ivec3 pos(worldX, worldY, worldZ);
    auto deltaChange = deltaStorage.getChange(pos); // getChange возвращает std::optional<BlockType>
    if (deltaChange) {
        // Нашли изменение в дельте, возвращаем его
        return *deltaChange; // Разыменовываем optional
    }
    else {
        // 2. В дельте нет, получаем базовый тип от генератора
        return getBaseBlockType(worldX, worldY, worldZ);
    }
}
// *** КОНЕЦ ИЗМЕНЕНИЙ getBlockType ***


// --- Установка типа блока (Изменение Дельты + Пометка чанков) ---
// *** ИЗМЕНЕНО: Не использует Chunk::setBlock ***
bool World::setBlockType(int worldX, int worldY, int worldZ, BlockType type) {
    // 1. Проверка границ мира
    if (worldX < 0 || worldX >= worldSizeX ||
        worldY < 0 || worldY >= worldSizeY ||
        worldZ < 0 || worldZ >= worldSizeZ) {
        return false; // Нельзя ставить блоки вне мира
    }

    glm::ivec3 pos(worldX, worldY, worldZ);

    // 2. Получаем базовый тип блока для этой позиции от генератора
    BlockType baseType = getBaseBlockType(worldX, worldY, worldZ);

    // 3. Получаем ТЕКУЩИЙ тип блока (учитывая дельту) с помощью нашего же метода
    BlockType currentType = this->getBlockType(worldX, worldY, worldZ); // Используем getBlockType

    // 4. Если новый тип совпадает с текущим, ничего не делаем
    if (type == currentType) {
        return false;
    }

    // 5. Записываем изменение в дельту
    // setChange сам обработает случай, если type == baseType (удалит запись)
    deltaStorage.setChange(pos, type, baseType);
    std::cout << "World: Set block (" << pos.x << "," << pos.y << "," << pos.z << ") to type "
        << static_cast<int>(type) << " in delta. Delta size: " << deltaStorage.getChangeCount() << std::endl;


    // 6. Пометка АКТИВНЫХ чанков для обновления меша
    // Находим чанк, которому принадлежит этот блок (в кэше активных)
    Chunk* targetChunk = getActiveChunk(worldX, worldZ); // Используем getActiveChunk
    if (targetChunk) {
        targetChunk->markForMeshUpdate(); // Помечаем основной чанк, если он загружен
        // std::cout << "DEBUG: Marked active chunk (" << targetChunk->getChunkX() << "," << targetChunk->getChunkZ() << ") for update." << std::endl;
    }

    // Помечаем АКТИВНЫХ соседей, если блок на границе
    glm::ivec3 localCoords = worldToLocalCoords(worldX, worldY, worldZ);
    int localX = localCoords.x;
    int localZ = localCoords.z;

    // Функция для пометки АКТИВНОГО соседа
    auto markActiveNeighborIfNeeded = [&](int dx, int dz) {
        Chunk* neighbor = getActiveChunk(worldX + dx, worldZ + dz); // Ищем в кэше активных
        if (neighbor) { // Помечаем, только если сосед загружен
            neighbor->markForMeshUpdate();
            // std::cout << "DEBUG: Marked active neighbor chunk (" << neighbor->getChunkX() << "," << neighbor->getChunkZ() << ") for update." << std::endl;
        }
        };

    if (localX == 0) markActiveNeighborIfNeeded(-1, 0); // Левый
    if (localX == Chunk::CHUNK_WIDTH - 1) markActiveNeighborIfNeeded(1, 0); // Правый
    if (localZ == 0) markActiveNeighborIfNeeded(0, -1); // Задний
    if (localZ == Chunk::CHUNK_DEPTH - 1) markActiveNeighborIfNeeded(0, 1); // Передний
    // Диагонали пока не трогаем для простоты

    return true; // Изменение внесено (в дельту)
}

Chunk* World::loadChunk(int chunkX, int chunkZ) {
    glm::ivec2 chunkPos(chunkX, chunkZ);

    // Проверка границ мира (важно!)
    if (chunkX < 0 || chunkX >= chunksX || chunkZ < 0 || chunkZ >= chunksZ) {
        // std::cerr << "Warning: Attempt to load chunk outside world bounds: (" << chunkX << "," << chunkZ << ")" << std::endl;
        return nullptr;
    }

    // Ищем чанк в кэше активных чанков
    auto it = activeChunks.find(chunkPos);
    if (it != activeChunks.end()) {
        // --- Чанк уже загружен ---
        // std::cout << "DEBUG: Chunk (" << chunkX << "," << chunkZ << ") already loaded." << std::endl;
        // Просто возвращаем указатель на существующий
        return it->second.get();
    }
    else {
        // --- Чанка нет в кэше, создаем и добавляем ---
        std::cout << "DEBUG: World::loadChunk - Loading NEW chunk (" << chunkX << "," << chunkZ << ")" << std::endl;

        // Создаем новый объект Chunk
        auto newChunk = std::make_unique<Chunk>(chunkX, chunkZ);
        if (!newChunk) {
            std::cerr << "ERROR::WORLD: Failed to create new chunk object for (" << chunkX << "," << chunkZ << ")" << std::endl;
            return nullptr;
        }

        // Получаем сырой указатель ПЕРЕД перемещением владения
        Chunk* rawPtr = newChunk.get();

        // *** КЛЮЧЕВОЙ МОМЕНТ: Добавляем созданный чанк в карту activeChunks ***
        // Используем emplace, который переместит unique_ptr
        auto insertResult = activeChunks.emplace(chunkPos, std::move(newChunk));

        // Проверяем, удалась ли вставка (на всякий случай)
        if (!insertResult.second) {
            std::cerr << "ERROR::WORLD: Failed to emplace new chunk into activeChunks map for (" << chunkX << "," << chunkZ << ")" << std::endl;
            // В этом случае rawPtr может быть невалидным, если emplace не удалось
            return nullptr;
        }

        // Устанавливаем флаг, что данные ЕЩЕ НЕ загружены (если есть асинхронная загрузка данных)
        // В нашей текущей модели данные блоков не хранятся в чанке,
        // поэтому флаг isDataLoaded не так важен для самого чанка,
        // но он используется в AsyncChunkManager::updateChunkLoading.
        // Установим его в true здесь, предполагая, что дельта/генератор всегда доступны.
        rawPtr->isDataLoaded = true; // <<<--- Устанавливаем флаг
        rawPtr->needsMeshUpdate = true; // <<<--- Новый чанк всегда требует меша

        std::cout << "DEBUG: World::loadChunk - Chunk (" << chunkX << "," << chunkZ << ") loaded and added to cache. Cache size: " << activeChunks.size() << std::endl;

        // Возвращаем указатель на только что созданный и добавленный чанк
        return rawPtr;
    }
}

// --- unloadChunk ---
void World::unloadChunk(int chunkX, int chunkZ) {
    glm::ivec2 chunkPos(chunkX, chunkZ);
    auto it = activeChunks.find(chunkPos);
    if (it != activeChunks.end()) {
        activeChunks.erase(it); // unique_ptr удалит Chunk
    }
}

// --- isChunkLoaded ---
bool World::isChunkLoaded(int chunkX, int chunkZ) const {
     glm::ivec2 chunkPos(chunkX, chunkZ);
     return activeChunks.count(chunkPos) > 0;
}

// --- getActiveChunk ---
const Chunk* World::getActiveChunk(int worldX, int worldZ) const {
    glm::ivec2 chunkPos = worldToChunkCoords(worldX, worldZ);
    auto it = activeChunks.find(chunkPos);
    return (it != activeChunks.end()) ? it->second.get() : nullptr;
}
Chunk* World::getActiveChunk(int worldX, int worldZ) {
    glm::ivec2 chunkPos = worldToChunkCoords(worldX, worldZ);
    auto it = activeChunks.find(chunkPos);
    return (it != activeChunks.end()) ? it->second.get() : nullptr;
}