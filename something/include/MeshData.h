#pragma once

#include "Mesh.h" // Для определения Vertex
#include <vector>
#include <glm/glm.hpp> // Можно убрать, если не нужен

// Простая структура для хранения сгенерированных данных меша
// перед загрузкой в OpenGL буферы.
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<GLuint> indices;
    // Можно добавить AABB меша, если нужно
};