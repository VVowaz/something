#include "Chunk.h"
#include "World.h"
#include "Mesh.h"
#include "Block.h"
#include "MeshData.h" // Включаем MeshData
#include <stdexcept>
#include <iostream>
#include <vector>
#include <utility>   // Для std::move
#include <memory>    // Для std::make_shared

// Статические члены
const int Chunk::CHUNK_WIDTH;
const int Chunk::CHUNK_HEIGHT;
const int Chunk::CHUNK_DEPTH;

Chunk::Chunk(int chunkX, int chunkZ)
    : chunkXPos(chunkX), chunkZPos(chunkZ),
    needsMeshUpdate(true), // Изначально требует генерации
    hasMeshGPU(false),     // Меша в GPU пока нет
    isDataLoaded(false),   // Данные пока не загружены
    isGeneratingMesh(false),// Меш пока не генерируется
    chunkMesh(nullptr)
{
    blocks.resize(CHUNK_WIDTH, std::vector<std::vector<BlockType>>(CHUNK_HEIGHT, std::vector<BlockType>(CHUNK_DEPTH, BlockType::Air)));
    calculateAABB();
}

// Деструктор
Chunk::~Chunk() {}

// *** НОВЫЙ МЕТОД: Расчет AABB ***
void Chunk::calculateAABB() {
    glm::ivec3 minPos = getMinWorldPos(); // Мировой угол
    boundingBox.min = glm::vec3(minPos.x, minPos.y, minPos.z);
    boundingBox.max = glm::vec3(minPos.x + CHUNK_WIDTH,
        minPos.y + CHUNK_HEIGHT, // Используем высоту чанка
        minPos.z + CHUNK_DEPTH);
}

// *** НОВЫЙ МЕТОД: Получение AABB ***
AABB Chunk::getAABB() const {
    return boundingBox;
}

// isLocalCoordValid
bool Chunk::isLocalCoordValid(int x, int y, int z) const {
    return x >= 0 && x < CHUNK_WIDTH && y >= 0 && y < CHUNK_HEIGHT && z >= 0 && z < CHUNK_DEPTH;
}

// getBlock
BlockType Chunk::getBlock(int localX, int localY, int localZ) const {
    if (!isLocalCoordValid(localX, localY, localZ)) return BlockType::Air;
    return blocks[localX][localY][localZ];
}

// setBlock
void Chunk::setBlock(int localX, int localY, int localZ, BlockType type) {
    if (isLocalCoordValid(localX, localY, localZ)) {
        if (blocks[localX][localY][localZ] != type) {
            blocks[localX][localY][localZ] = type;
            // Помечаем для обновления и сбрасываем флаг наличия меша в GPU
            this->markForMeshUpdate();
            // TODO: Пометить соседние чанки
        }
    }
    else { /* warning */ }
}

void Chunk::fillChunkData(const WorldDataStructure& worldData, int worldWidth, int worldDepth) {
    int startWorldX = chunkXPos * CHUNK_WIDTH;
    int startWorldZ = chunkZPos * CHUNK_DEPTH;
    int worldHeight = CHUNK_HEIGHT; // Высота мира/чанка

    // Проверка размеров worldData (как раньше)
    if (worldData.empty() || worldData.size() != worldWidth ||
        worldData[0].empty() || worldData[0].size() != worldHeight || // Сверяем с высотой чанка/мира
        worldData[0][0].empty() || worldData[0][0].size() != worldDepth)
    {
        std::cerr << "ERROR::CHUNK::FILL: Invalid worldData dimensions! Expected "
            << worldWidth << "x" << worldHeight << "x" << worldDepth
            << ", Got vector with size " << worldData.size() << std::endl;
        // ... (заполнение камнем) ...
        this->needsMeshUpdate = true;
        this->isDataLoaded = true;
        return;
    }

    // Копирование данных
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        // Проверка индекса Y для worldData (хотя он должен совпадать с CHUNK_HEIGHT)
        if (y < 0 || y >= worldData[0].size()) {
            std::cerr << "FATAL::CHUNK::FILL: Index Y out of bounds for worldData! y=" << y << std::endl;
            continue; // Пропускаем этот y
        }
        for (int z_local = 0; z_local < CHUNK_DEPTH; ++z_local) {
            int currentWorldZ = startWorldZ + z_local;
            // Проверка индекса Z для worldData
            if (currentWorldZ < 0 || currentWorldZ >= worldData[0][0].size()) {
                std::cerr << "FATAL::CHUNK::FILL: Index Z out of bounds for worldData! worldZ=" << currentWorldZ << std::endl;
                continue; // Пропускаем этот z
            }
            for (int x_local = 0; x_local < CHUNK_WIDTH; ++x_local) {
                int currentWorldX = startWorldX + x_local;
                // Проверка индекса X для worldData
                if (currentWorldX < 0 || currentWorldX >= worldData.size()) {
                    std::cerr << "FATAL::CHUNK::FILL: Index X out of bounds for worldData! worldX=" << currentWorldX << std::endl;
                    continue; // Пропускаем этот x
                }

                // *** СТРОКА, ГДЕ МОЖЕТ БЫТЬ ОШИБКА ДОСТУПА ***
                // Доступ к локальным блокам (проверка локальных индексов не нужна, т.к. цикл по ним идет)
                // Доступ к данным мира (индексы проверены выше)
                try {
                    this->blocks[x_local][y][z_local] = worldData.at(currentWorldX).at(y).at(currentWorldZ); // Используем .at() для доп. проверки
                }
                catch (const std::out_of_range& oor) {
                    std::cerr << "FATAL::CHUNK::FILL: Out of range error accessing worldData at ["
                        << currentWorldX << "][" << y << "][" << currentWorldZ << "]. " << oor.what() << std::endl;
                    // Можно установить блок в Air или Stone в случае ошибки
                    this->blocks[x_local][y][z_local] = BlockType::Air;
                }
            }
        }
    }
    this->isDataLoaded = true;
    this->needsMeshUpdate = true;
}

// Приватный метод: выполняет фактическую генерацию
std::shared_ptr<MeshData> Chunk::generateMeshInternalData(const World& world) {
    auto meshData = std::make_shared<MeshData>();
    // std::cout << "Chunk (" << chunkXPos << "," << chunkZPos << "): Generating mesh..." << std::endl;
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    GLuint currentIndex = 0;
    glm::ivec3 chunkMin = getMinWorldPos();

    // Данные граней (vertex data for a cube face: pos(3), norm(3), uv(2))
    const GLfloat faceVertices[][8] = {
        // Positions           // Normals       // TexCoords
        // Front face (+Z) Normal (0, 0, 1)
        {-0.5f, -0.5f,  0.5f,  /*0,0,1,*/ 0.0f, 0.0f}, // 0
        { 0.5f, -0.5f,  0.5f,  /*0,0,1,*/ 1.0f, 0.0f}, // 1
        { 0.5f,  0.5f,  0.5f,  /*0,0,1,*/ 1.0f, 1.0f}, // 2
        {-0.5f,  0.5f,  0.5f,  /*0,0,1,*/ 0.0f, 1.0f}, // 3
        // Back face (-Z) Normal (0, 0, -1)
        { 0.5f, -0.5f, -0.5f,  /*0,0,-1,*/0.0f, 0.0f}, // 4
        {-0.5f, -0.5f, -0.5f,  /*0,0,-1,*/1.0f, 0.0f}, // 5
        {-0.5f,  0.5f, -0.5f,  /*0,0,-1,*/1.0f, 1.0f}, // 6
        { 0.5f,  0.5f, -0.5f,  /*0,0,-1,*/0.0f, 1.0f}, // 7
        // Left face (-X) Normal (-1, 0, 0)
        {-0.5f, -0.5f, -0.5f,  /*-1,0,0,*/0.0f, 0.0f}, // 8
        {-0.5f, -0.5f,  0.5f,  /*-1,0,0,*/1.0f, 0.0f}, // 9
        {-0.5f,  0.5f,  0.5f,  /*-1,0,0,*/1.0f, 1.0f}, // 10
        {-0.5f,  0.5f, -0.5f,  /*-1,0,0,*/0.0f, 1.0f}, // 11
        // Right face (+X) Normal (1, 0, 0)
        { 0.5f, -0.5f,  0.5f,  /*1,0,0,*/ 0.0f, 0.0f}, // 12
        { 0.5f, -0.5f, -0.5f,  /*1,0,0,*/ 1.0f, 0.0f}, // 13
        { 0.5f,  0.5f, -0.5f,  /*1,0,0,*/ 1.0f, 1.0f}, // 14
        { 0.5f,  0.5f,  0.5f,  /*1,0,0,*/ 0.0f, 1.0f}, // 15
         // Bottom face (-Y) Normal (0, -1, 0)
        {-0.5f, -0.5f, -0.5f,  /*0,-1,0,*/0.0f, 0.0f}, // 16
        { 0.5f, -0.5f, -0.5f,  /*0,-1,0,*/1.0f, 0.0f}, // 17
        { 0.5f, -0.5f,  0.5f,  /*0,-1,0,*/1.0f, 1.0f}, // 18
        {-0.5f, -0.5f,  0.5f,  /*0,-1,0,*/0.0f, 1.0f}, // 19
        // Top face (+Y) Normal (0, 1, 0)
        {-0.5f,  0.5f,  0.5f,  /*0,1,0,*/ 0.0f, 0.0f}, // 20
        { 0.5f,  0.5f,  0.5f,  /*0,1,0,*/ 1.0f, 0.0f}, // 21
        { 0.5f,  0.5f, -0.5f,  /*0,1,0,*/ 1.0f, 1.0f}, // 22
        {-0.5f,  0.5f, -0.5f,  /*0,1,0,*/ 0.0f, 1.0f}  // 23
    };
     // Normals for each face direction
     const glm::vec3 faceNormals[] = {
        { 0.0f,  0.0f,  1.0f}, // Front (+Z)
        { 0.0f,  0.0f, -1.0f}, // Back (-Z)
        {-1.0f,  0.0f,  0.0f}, // Left (-X)
        { 1.0f,  0.0f,  0.0f}, // Right (+X)
        { 0.0f, -1.0f,  0.0f}, // Bottom (-Y)
        { 0.0f,  1.0f,  0.0f}  // Top (+Y)
    };
    // Vertex indices for a single face (quad = 2 triangles)
    const GLuint faceIndices[] = { 0, 1, 2, 2, 3, 0 };


    // Lambda to add a face to the mesh vectors
    auto addFace = [&](int x, int y, int z, int faceDirIndex) {
        glm::vec3 blockLocalCornerPos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        glm::vec3 normal = faceNormals[faceDirIndex];
        int vertexDataOffset = faceDirIndex * 4 * 5;
        for (int i = 0; i < 4; ++i) {
            int currentVertexOffset = vertexDataOffset + i * 5;
            Vertex v;
            // Позиция вершины вычисляется относительно МИРОВОГО УГЛА ЧАНКА (0,0,0 чанка = chunkMin в мире)
            // Это НЕПРАВИЛЬНО для меша чанка! Позиции должны быть ЛОКАЛЬНЫМИ для чанка (0..16)
            // ИСПРАВЛЕНИЕ: Используем только локальные координаты блока + смещение вершины
            v.Position = blockLocalCornerPos                     // Локальный угол блока (0..15)
                + glm::vec3(0.5f, 0.5f, 0.5f)           // Смещение к центру блока
                + glm::vec3(faceVertices[faceDirIndex * 4 + i][0], // Смещение вершины отн. центра
                    faceVertices[faceDirIndex * 4 + i][1],
                    faceVertices[faceDirIndex * 4 + i][2]);
            v.Normal = normal;
            v.TexCoords = glm::vec2(
                faceVertices[currentVertexOffset][3], // UV.x
                faceVertices[currentVertexOffset][4]  // UV.y
            );
            meshData->vertices.push_back(v); // Добавляем в структуру данных
        }
        for (int i = 0; i < 6; ++i) { meshData->indices.push_back(currentIndex + faceIndices[i]); }
        currentIndex += 4;
        };

    // Цикл по блокам и проверка соседей (как раньше)
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {
                BlockType currentBlock = blocks[x][y][z];
                if (!isBlockVisible(currentBlock)) continue;
                int worldX = chunkMin.x + x; int worldY = y; int worldZ = chunkMin.z + z;
                if (isBlockTransparent(world.getBlockType(worldX, worldY, worldZ + 1))) addFace(x, y, z, 0);
                if (isBlockTransparent(world.getBlockType(worldX, worldY, worldZ - 1))) addFace(x, y, z, 1);
                if (isBlockTransparent(world.getBlockType(worldX - 1, worldY, worldZ))) addFace(x, y, z, 2);
                if (isBlockTransparent(world.getBlockType(worldX + 1, worldY, worldZ))) addFace(x, y, z, 3);
                if (isBlockTransparent(world.getBlockType(worldX, worldY - 1, worldZ))) addFace(x, y, z, 4);
                if (isBlockTransparent(world.getBlockType(worldX, worldY + 1, worldZ))) addFace(x, y, z, 5);
            }
        }
    }

    // Возвращаем указатель на данные (может быть пустым, если нет видимых граней)
    if (meshData->vertices.empty() || meshData->indices.empty()) {
        return nullptr; // Возвращаем null, если меш пуст
    }
    return meshData;
}

void Chunk::uploadMeshToGPU(std::shared_ptr<MeshData> meshData) {
    if (!meshData || meshData->vertices.empty() || meshData->indices.empty()) {
        // Если данные пусты, просто очищаем существующий меш (если он был)
        chunkMesh.reset();
        hasMeshGPU = false; // Указываем, что меша в GPU нет
        // std::cout << "Chunk (" << chunkXPos << "," << chunkZPos << "): No mesh data to upload, cleared GPU mesh." << std::endl;
        return;
    }

    // Создаем новый объект Mesh с полученными данными
    // Старый объект Mesh (если был) будет автоматически удален unique_ptr.reset()
    try {
        chunkMesh = std::make_unique<Mesh>(meshData->vertices, meshData->indices);
        if (chunkMesh->isValid()) {
            hasMeshGPU = true; // Устанавливаем флаг, что меш теперь в GPU
            // std::cout << "Chunk (" << chunkXPos << "," << chunkZPos << "): Mesh uploaded to GPU ("
            //           << meshData->vertices.size() << " V, " << meshData->indices.size() << " I)." << std::endl;
        }
        else {
            std::cerr << "ERROR::CHUNK::UPLOAD: Created mesh is invalid for chunk (" << chunkXPos << "," << chunkZPos << ")" << std::endl;
            chunkMesh.reset(); // Удаляем невалидный меш
            hasMeshGPU = false;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR::CHUNK::UPLOAD: Exception during Mesh creation: " << e.what() << std::endl;
        chunkMesh.reset();
        hasMeshGPU = false;
    }
    catch (...) {
        std::cerr << "ERROR::CHUNK::UPLOAD: Unknown exception during Mesh creation." << std::endl;
        chunkMesh.reset();
        hasMeshGPU = false;
    }
}

// Выгрузка меша из GPU
void Chunk::unloadMesh() {
    if (chunkMesh) {
        chunkMesh.reset(); // Удаляем Mesh (деструктор освободит буферы OpenGL)
        hasMeshGPU = false; // Сбрасываем флаг
        needsMeshUpdate = true; // Помечаем, что нужно перестроить, если понадобится снова
        // std::cout << "Chunk (" << chunkXPos << "," << chunkZPos << "): Mesh unloaded from GPU." << std::endl;
    }
}

glm::ivec3 Chunk::getMinWorldPos() const {
    // Умножаем индекс чанка на его размер в блоках
    return glm::ivec3(this->chunkXPos * CHUNK_WIDTH,   // this-> необязателен
        0,                               // Мировой Y = 0
        this->chunkZPos * CHUNK_DEPTH);  // this-> необязателен
}