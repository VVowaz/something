#include "World.h"   // Включаем свой заголовок
#include "Chunk.h"   // <<<--- ВКЛЮЧАЕМ ПОЛНОЕ ОПРЕДЕЛЕНИЕ CHUNK ЗДЕСЬ
#include <cmath>
#include <iostream>
#include <utility>
#include <stdexcept> // Для runtime_error

// Конструктор
World::World(int sizeX, int sizeY, int sizeZ)
    : worldSizeX(sizeX), worldSizeY(sizeY), worldSizeZ(sizeZ)
{
    // Проверка размеров
    if (worldSizeX <= 0 || worldSizeY <= 0 || worldSizeZ <= 0 ||
        worldSizeX % Chunk::CHUNK_WIDTH != 0 ||
        worldSizeY != Chunk::CHUNK_HEIGHT || // Проверяем совпадение высоты
        worldSizeZ % Chunk::CHUNK_DEPTH != 0)
    {
        std::cerr << "ERROR::WORLD: Invalid world dimensions or mismatch with chunk dimensions." << std::endl;
        throw std::runtime_error("Invalid world dimensions for chunking.");
    }
    chunksX = worldSizeX / Chunk::CHUNK_WIDTH;
    chunksZ = worldSizeZ / Chunk::CHUNK_DEPTH;
    std::cout << "World object created. Dimensions (Blocks): " << worldSizeX << "x" << worldSizeY << "x" << worldSizeZ
        << ". Dimensions (Chunks): " << chunksX << "x" << chunksZ << "." << std::endl;

    createChunks(); // Создаем пустые чанки
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

// getBlockType
BlockType World::getBlockType(int worldX, int worldY, int worldZ) const {
    if (worldY < 0 || worldY >= worldSizeY) return BlockType::Air;
    const Chunk* targetChunk = getChunk(worldX, worldZ);
    if (targetChunk) {
        glm::ivec3 localCoords = worldToLocalCoords(worldX, worldY, worldZ);
        return targetChunk->getBlock(localCoords.x, localCoords.y, localCoords.z);
    }
    else {
        return BlockType::Air;
    }
}

void World::populateChunksInternal(const WorldDataType& worldData) { // Принимает const&
    std::cout << "World: Populating chunks internally..." << std::endl;
    // Итерация по карте чанков
    for (auto const& [pos, chunkPtr] : chunks) { // Цикл, НЕ рекурсия
        if (chunkPtr) {
            // Вызов метода ДРУГОГО класса (Chunk)
            chunkPtr->fillChunkData(worldData, worldSizeX, worldSizeZ);
        }
    }
    std::cout << "World: Chunks populated internally." << std::endl;
}

// populate - вызывает populateChunksInternal
void World::populate(WorldDataType&& worldData) {

    // Перемещаем данные во временную локальную переменную, чтобы владеть ими здесь.
    // Это может быть избыточно, но явно показывает владение и позволяет передать const& дальше.
    WorldDataType localWorldData = std::move(worldData);
    std::cout << "DEBUG::WORLD::POPULATE: Moved world data locally." << std::endl;

    // Вызываем внутреннее заполнение, передавая const ссылку на локальные данные
    populateChunksInternal(localWorldData);
}

// *** НОВАЯ РЕАЛИЗАЦИЯ: setBlockType ***
bool World::setBlockType(int worldX, int worldY, int worldZ, BlockType type) {
    // 1. Проверка выхода за границы по высоте
    if (worldY < 0 || worldY >= worldSizeY) {
        // std::cerr << "Warning: Attempt to set block outside world Y bounds: " << worldY << std::endl;
        return false;
    }

    // 2. Находим целевой чанк (не const версия)
    Chunk* targetChunk = getChunk(worldX, worldZ);

    if (targetChunk) {
        // 3. Конвертируем мировые координаты в локальные
        glm::ivec3 localCoords = worldToLocalCoords(worldX, worldY, worldZ);

        // 4. Получаем ТЕКУЩИЙ тип блока
        BlockType currentType = targetChunk->getBlock(localCoords.x, localCoords.y, localCoords.z);

        // 5. Устанавливаем новый тип, если он отличается
        if (currentType != type) {
            targetChunk->setBlock(localCoords.x, localCoords.y, localCoords.z, type);
            // setBlock внутри Chunk уже помечает needsMeshUpdate = true

            // *** 6. Пометка СОСЕДНИХ чанков для обновления (ВАЖНО!) ***
            // Если блок находится на границе чанка, нужно перестроить меш и соседнего чанка.
            int localX = localCoords.x;
            int localZ = localCoords.z;
            bool onEdgeX_Neg = (localX == 0);
            bool onEdgeX_Pos = (localX == Chunk::CHUNK_WIDTH - 1);
            bool onEdgeZ_Neg = (localZ == 0);
            bool onEdgeZ_Pos = (localZ == Chunk::CHUNK_DEPTH - 1);

            // Проверяем соседей по X
            if (onEdgeX_Neg) { Chunk* neighbor = getChunk(worldX - 1, worldZ); if (neighbor) neighbor->markForMeshUpdate(); }
            if (onEdgeX_Pos) { Chunk* neighbor = getChunk(worldX + 1, worldZ); if (neighbor) neighbor->markForMeshUpdate(); }
            // Проверяем соседей по Z
            if (onEdgeZ_Neg) { Chunk* neighbor = getChunk(worldX, worldZ - 1); if (neighbor) neighbor->markForMeshUpdate(); }
            if (onEdgeZ_Pos) { Chunk* neighbor = getChunk(worldX, worldZ + 1); if (neighbor) neighbor->markForMeshUpdate(); }
            // Проверяем соседей по диагонали (тоже влияют на стыки граней)
            if (onEdgeX_Neg && onEdgeZ_Neg) { Chunk* neighbor = getChunk(worldX - 1, worldZ - 1); if (neighbor) neighbor->markForMeshUpdate(); }
            if (onEdgeX_Neg && onEdgeZ_Pos) { Chunk* neighbor = getChunk(worldX - 1, worldZ + 1); if (neighbor) neighbor->markForMeshUpdate(); }
            if (onEdgeX_Pos && onEdgeZ_Neg) { Chunk* neighbor = getChunk(worldX + 1, worldZ - 1); if (neighbor) neighbor->markForMeshUpdate(); }
            if (onEdgeX_Pos && onEdgeZ_Pos) { Chunk* neighbor = getChunk(worldX + 1, worldZ + 1); if (neighbor) neighbor->markForMeshUpdate(); }


            return true; // Блок изменен
        }
        else {
            return false; // Тип не изменился
        }
    }
    else {
        // Попытка установить блок вне загруженных/существующих чанков
        // std::cerr << "Warning: Attempt to set block outside world X/Z bounds: " << worldX << "," << worldZ << std::endl;
        return false;
    }
}