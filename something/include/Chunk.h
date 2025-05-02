#pragma once

#include "Block.h"
#include "Mesh.h"
#include "MeshData.h" // Включаем новую структуру
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <atomic> // Для атомарных флагов

struct AABB { glm::vec3 min, max; };
class World;

class Chunk {
public:
    static const int CHUNK_WIDTH = 16;
    static const int CHUNK_HEIGHT = 50;
    static const int CHUNK_DEPTH = 16;
    using ChunkDataType = std::vector<std::vector<std::vector<BlockType>>>;

    Chunk(int chunkX, int chunkZ);
    ~Chunk();

    Chunk(const Chunk&) = delete; Chunk& operator=(const Chunk&) = delete;
    Chunk(Chunk&&) = default; Chunk& operator=(Chunk&&) = default;

    BlockType getBlock(int localX, int localY, int localZ) const;
    void setBlock(int localX, int localY, int localZ, BlockType type);
    void fillChunkData(const WorldDataStructure& worldData, int worldWidth, int worldDepth);

    int getChunkX() const { return chunkXPos; }
    int getChunkZ() const { return chunkZPos; }
    glm::ivec3 getMinWorldPos() const;
    AABB getAABB() const;

    // --- Новые и измененные методы ---
    // Генерация данных меша (вызывается из рабочего потока)
    // Возвращает данные или пустой shared_ptr при ошибке/пустом меше
    std::shared_ptr<MeshData> generateMeshInternalData(const World& world);

    // Загрузка сгенерированных данных в OpenGL (вызывается из главного потока)
    void uploadMeshToGPU(std::shared_ptr<MeshData> meshData);

    // Выгрузка меша (без изменений)
    void unloadMesh();

    const Mesh* getMesh() const { return chunkMesh.get(); }

    // Атомарные флаги состояния
    std::atomic<bool> needsMeshUpdate = true; // Нужна ли перегенерация данных?
    std::atomic<bool> hasMeshGPU = false;     // Есть ли меш в GPU?
    std::atomic<bool> isDataLoaded = false;   // Загружены ли данные блока? (Пока всегда true после populate)
    std::atomic<bool> isGeneratingMesh = false; // Генерируется ли меш сейчас (для избежания дублирования задач)

    void markForMeshUpdate() { needsMeshUpdate = true; hasMeshGPU = false; } // Сбрасываем флаг GPU при пометке


private:
    int chunkXPos, chunkZPos;
    ChunkDataType blocks;
    std::unique_ptr<Mesh> chunkMesh; // Хранит OpenGL буферы
    // bool needsMeshUpdate; // Заменено на atomic
    AABB boundingBox;

    bool isLocalCoordValid(int x, int y, int z) const;
    void calculateAABB();
    // generateMeshInternal больше не нужен, логика в generateMeshInternalData
};