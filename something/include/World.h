#pragma once

#include "Block.h"
#include "Chunk.h"
#include "WorldGenerator.h" // <<<--- Включаем для WorldDataType
#include <vector>
#include <memory>
#include <map>
#include <glm/glm.hpp>
#include "DeltaStorage.h"   // Включаем для хранения дельты
#include <functional>       // Для std::function

struct ivec2_less {
    // Проблема здесь! Метод должен быть const
    bool operator()(const glm::ivec2& a, const glm::ivec2& b) const { // <<<--- ДОБАВИТЬ const
        if (a.x != b.x) return a.x < b.x;
        return a.y < b.y; // Используем y для Z координаты чанка
    }
};

class World {
public:
    using WorldDataType = WorldDataStructure; // Из Block.h
    using SurfaceHeightFunction = WorldGenerator::SurfaceHeightFunction; // Из WorldGenerator.h
    using ChunkMap = std::map<glm::ivec2, std::unique_ptr<Chunk>, ivec2_less>;

    World(int sizeX, int sizeY, int sizeZ,
        SurfaceHeightFunction genFunc,
        DeltaStorage&& loadedDelta);
    ~World();
    World(const World&) = delete; World& operator=(const World&) = delete;
    World(World&&) = default; World& operator=(World&&) = default;

    void populate(WorldDataType&& worldData); // <<< Принимает rvalue-ссылку


    BlockType getBlockType(int worldX, int worldY, int worldZ) const;
    const Chunk* getChunk(int worldX, int worldZ) const;
    Chunk* getChunk(int worldX, int worldZ); // Не-const версия
    int getWidth() const { return worldSizeX; }
    int getHeight() const { return worldSizeY; }
    int getDepth() const { return worldSizeZ; }
    int getChunkWidth() const { return chunksX; }
    int getChunkDepth() const { return chunksZ; }
    const DeltaStorage& getDeltaStorage() const { return deltaStorage;}
    ChunkMap& getChunks() { return chunks; } // Не-const версия
    const ChunkMap& getChunks() const { return chunks; } // const версия

    bool setBlockType(int worldX, int worldY, int worldZ, BlockType type);

    Chunk* loadChunk(int chunkX, int chunkZ);
    void unloadChunk(int chunkX, int chunkZ);
    bool isChunkLoaded(int chunkX, int chunkZ) const;
    const Chunk* getActiveChunk(int worldX, int worldZ) const;
    Chunk* getActiveChunk(int worldX, int worldZ);
    ChunkMap& getActiveChunks() { return activeChunks; }
    const ChunkMap& getActiveChunks() const { return activeChunks; }

    void unloadAllMeshes(); // Без изменений

    SurfaceHeightFunction generationFunction; // Функция для генерации базового мира

    glm::ivec2 worldToChunkCoords(int worldX, int worldZ) const;
    glm::ivec3 worldToLocalCoords(int worldX, int worldY, int worldZ) const;

private:
    int worldSizeX, worldSizeY, worldSizeZ;
    int chunksX, chunksZ;
    ChunkMap chunks;

    DeltaStorage deltaStorage; // Хранит только измененные блоки

    // --- Кэш активных чанков ---
    ChunkMap activeChunks; // Хранит только загруженные в память чанки

    void createChunks();
    void populateChunksInternal(const WorldDataType& worldData); // Принимает const&
    // void generateChunkMeshesInternal(); // Больше не нужен
    BlockType getBaseBlockType(int, int, int) const;
};