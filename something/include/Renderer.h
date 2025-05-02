#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <array> // Для плоскостей фрустума
#include "Camera.h" // Включаем для определения Plane
#include "Chunk.h"  // Включаем для определения AABB

// Прямые объявления
class World;
class Camera;
class Shader;
class Mesh; // Используем Mesh вместо прямого VAO/VBO
class Chunk;

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void prepareFrame() const;
    // *** ИСПРАВЛЕНА СИГНАТУРА ***
    void renderWorld(World& world, const Camera& camera, Shader& blockShader, const std::array<Plane, 6>& frustumPlanes) const;
    void renderGrid(Shader& lineShader, GLuint gridVAO, GLsizei gridVertexCount, const glm::mat4& view, const glm::mat4& projection) const;
    void renderDebugInfo(const Camera& camera, Shader& lineShader, GLuint debugAxesVAO, GLsizei debugAxesVertexCount) const;
    // Настройки рендерера
    // *** НОВЫЙ МЕТОД: Проверка видимости AABB ***
    // Возвращает true, если AABB находится внутри или пересекает фрустум
    bool isAABBInFrustum(const AABB& box, const std::array<Plane, 6>& planes) const;
};