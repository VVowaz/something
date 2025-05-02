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