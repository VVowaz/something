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


// Приватный метод: выполняет фактическую генерацию
std::shared_ptr<MeshData> Chunk::generateMeshInternalData(const World& world) {
    // Создаем объект для хранения данных меша
    auto meshData = std::make_shared<MeshData>();
    // Резервируем память в векторах для предполагаемого среднего размера меша
    // Это может немного ускорить добавление элементов, избегая частых реалокаций.
    // Значения подобраны примерно, можно настроить.
    meshData->vertices.reserve(CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT * 3); // Примерно половина блоков видима, 6 граней, 4 верш/грань -> *12? Грубо *3
    meshData->indices.reserve(CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT * 9); // Примерно *18? Грубо *9

    GLuint currentIndex = 0; // Индекс для следующей добавляемой вершины
    glm::ivec3 chunkMin = getMinWorldPos(); // Мировые координаты угла (0,0,0) этого чанка

    // --- Данные для граней (как раньше) ---
    // Позиции вершин грани относительно центра блока (-0.5 до +0.5), UV координаты
    const GLfloat faceVertices[][5] = {
        // PosX, PosY, PosZ, TexU, TexV
        {-0.5f, -0.5f,  0.5f, 0.0f, 0.0f}, { 0.5f, -0.5f,  0.5f, 1.0f, 0.0f}, { 0.5f,  0.5f,  0.5f, 1.0f, 1.0f}, {-0.5f,  0.5f,  0.5f, 0.0f, 1.0f}, // Front (+Z)
        { 0.5f, -0.5f, -0.5f, 0.0f, 0.0f}, {-0.5f, -0.5f, -0.5f, 1.0f, 0.0f}, {-0.5f,  0.5f, -0.5f, 1.0f, 1.0f}, { 0.5f,  0.5f, -0.5f, 0.0f, 1.0f}, // Back (-Z)
        {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f}, {-0.5f, -0.5f,  0.5f, 1.0f, 0.0f}, {-0.5f,  0.5f,  0.5f, 1.0f, 1.0f}, {-0.5f,  0.5f, -0.5f, 0.0f, 1.0f}, // Left (-X)
        { 0.5f, -0.5f,  0.5f, 0.0f, 0.0f}, { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f}, { 0.5f,  0.5f, -0.5f, 1.0f, 1.0f}, { 0.5f,  0.5f,  0.5f, 0.0f, 1.0f}, // Right (+X)
        {-0.5f, -0.5f, -0.5f, 0.0f, 0.0f}, { 0.5f, -0.5f, -0.5f, 1.0f, 0.0f}, { 0.5f, -0.5f,  0.5f, 1.0f, 1.0f}, {-0.5f, -0.5f,  0.5f, 0.0f, 1.0f}, // Bottom (-Y)
        {-0.5f,  0.5f,  0.5f, 0.0f, 0.0f}, { 0.5f,  0.5f,  0.5f, 1.0f, 0.0f}, { 0.5f,  0.5f, -0.5f, 1.0f, 1.0f}, {-0.5f,  0.5f, -0.5f, 0.0f, 1.0f}  // Top (+Y)
    };
    // Нормали для каждой грани
    const glm::vec3 faceNormals[] = {
       { 0.0f,  0.0f,  1.0f}, { 0.0f,  0.0f, -1.0f}, {-1.0f,  0.0f,  0.0f},
       { 1.0f,  0.0f,  0.0f}, { 0.0f, -1.0f,  0.0f}, { 0.0f,  1.0f,  0.0f}
    };
    // Индексы для квадрата
    const GLuint faceIndices[] = { 0, 1, 2, 2, 3, 0 };
    // --- Конец данных для граней ---


    // Лямбда для добавления грани (без изменений)
    auto addFace = [&](int x, int y, int z, int faceDirIndex) {
        // Локальная позиция УГЛА блока (0..15)
        glm::vec3 blockLocalCornerPos(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
        glm::vec3 normal = faceNormals[faceDirIndex];
        int vertexDataOffset = faceDirIndex * 4 * 5; // Смещение к данным нужной грани

        for (int i = 0; i < 4; ++i) { // Добавляем 4 вершины грани
            int currentVertexDataIndex = vertexDataOffset + i * 5; // Индекс в faceVertices
            Vertex v;
            // Позиция вершины = Локальный угол блока + смещение к центру + смещение вершины от центра
            v.Position = blockLocalCornerPos
                + glm::vec3(0.5f, 0.5f, 0.5f)
                + glm::vec3(faceVertices[faceDirIndex * 4 + i][0],
                    faceVertices[faceDirIndex * 4 + i][1],
                    faceVertices[faceDirIndex * 4 + i][2]);
            v.Normal = normal;
            v.TexCoords = glm::vec2(faceVertices[faceDirIndex * 4 + i][3],
                faceVertices[faceDirIndex * 4 + i][4]);
            meshData->vertices.push_back(v);
        }
        // Добавляем индексы для этой грани
        for (int i = 0; i < 6; ++i) {
            meshData->indices.push_back(currentIndex + faceIndices[i]);
        }
        currentIndex += 4; // Увеличиваем базовый индекс для следующей грани
        };


    // --- Основной цикл мешинга ---
    // Проходим по всем ЛОКАЛЬНЫМ координатам внутри чанка
    for (int y = 0; y < CHUNK_HEIGHT; ++y) {
        for (int z = 0; z < CHUNK_DEPTH; ++z) {
            for (int x = 0; x < CHUNK_WIDTH; ++x) {

                // Вычисляем МИРОВЫЕ координаты текущего блока
                int worldX = chunkMin.x + x;
                int worldY = y; // Y совпадает
                int worldZ = chunkMin.z + z;

                // *** ИЗМЕНЕНИЕ: Получаем тип блока из World (учитывает дельту и генератор) ***
                BlockType currentBlock = world.getBlockType(worldX, worldY, worldZ);

                // Если текущий блок - воздух, пропускаем его, он не имеет граней
                if (!isBlockVisible(currentBlock)) {
                    continue;
                }

                // --- Проверяем 6 соседей по МИРОВЫМ координатам ---
                // Если сосед прозрачный (воздух), добавляем соответствующую грань ТЕКУЩЕГО блока

                // Сосед +Z (Передняя грань текущего блока)
                if (isBlockTransparent(world.getBlockType(worldX, worldY, worldZ + 1))) {
                    addFace(x, y, z, 0); // Добавляем грань +Z (индекс 0)
                }
                // Сосед -Z (Задняя грань текущего блока)
                if (isBlockTransparent(world.getBlockType(worldX, worldY, worldZ - 1))) {
                    addFace(x, y, z, 1); // Добавляем грань -Z (индекс 1)
                }
                // Сосед -X (Левая грань текущего блока)
                if (isBlockTransparent(world.getBlockType(worldX - 1, worldY, worldZ))) {
                    addFace(x, y, z, 2); // Добавляем грань -X (индекс 2)
                }
                // Сосед +X (Правая грань текущего блока)
                if (isBlockTransparent(world.getBlockType(worldX + 1, worldY, worldZ))) {
                    addFace(x, y, z, 3); // Добавляем грань +X (индекс 3)
                }
                // Сосед -Y (Нижняя грань текущего блока)
                if (isBlockTransparent(world.getBlockType(worldX, worldY - 1, worldZ))) {
                    addFace(x, y, z, 4); // Добавляем грань -Y (индекс 4)
                }
                // Сосед +Y (Верхняя грань текущего блока)
                if (isBlockTransparent(world.getBlockType(worldX, worldY + 1, worldZ))) {
                    addFace(x, y, z, 5); // Добавляем грань +Y (индекс 5)
                }
            } // конец x
        } // конец z
    } // конец y
    // --- Конец основного цикла мешинга ---


    // Возвращаем указатель на данные меша
    if (meshData->vertices.empty() || meshData->indices.empty()) {
        // std::cout << "Chunk (" << chunkXPos << "," << chunkZPos << "): Generated mesh data is empty." << std::endl;
        return nullptr; // Возвращаем null, если нет видимых граней
    }

    // std::cout << "Chunk (" << chunkXPos << "," << chunkZPos << "): Mesh data generated ("
    //           << meshData->vertices.size() << " V, " << meshData->indices.size() << " I)." << std::endl;
    return meshData; // Возвращаем умный указатель на данные
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