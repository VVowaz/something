#include "Renderer.h"
#include "World.h"   // Включаем для доступа к getChunks и Chunk
#include "Camera.h"
#include "Shader.h"
#include "Mesh.h"
#include "Block.h"
#include "Chunk.h"   // Включаем определение Chunk

#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

Renderer::Renderer() { std::cout << "Renderer created." << std::endl; }
Renderer::~Renderer() { std::cout << "Renderer destroyed." << std::endl; }

void Renderer::prepareFrame() const {
    glClearColor(0.5f, 0.8f, 1.0f, 1.0f); // Sky blue background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Проверка ошибок OpenGL в начале кадра
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        std::cerr << "OpenGL Error at frame start: " << err << std::endl;
    }
}

// *** НОВЫЙ МЕТОД: Проверка AABB против фрустума ***
// Источник: https://learnopengl.com/Guest-Articles/2021/Scene/Frustum-Culling
// Адаптированный метод проверки AABB-плоскость
bool Renderer::isAABBInFrustum(const AABB& box, const std::array<Plane, 6>& planes) const {
    // Проверяем каждую плоскость фрустума
    for (int i = 0; i < 6; ++i) {
        const Plane& plane = planes[i];
        // Находим "положительную" вершину AABB относительно нормали плоскости
        // Это вершина, которая дальше всего "выступает" в направлении нормали
        glm::vec3 positiveVertex = box.min;
        if (plane.normal.x >= 0) positiveVertex.x = box.max.x;
        if (plane.normal.y >= 0) positiveVertex.y = box.max.y;
        if (plane.normal.z >= 0) positiveVertex.z = box.max.z;

        // Проверяем, находится ли эта "положительная" (самая дальняя) вершина
        // с отрицательной стороны плоскости (т.е. полностью "за" ней).
        // Расстояние = dot(normal, point) + distance
        if (glm::dot(plane.normal, positiveVertex) + plane.distance < 0.0f) {
            // Если хотя бы для одной плоскости самая дальняя точка находится сзади,
            // то весь AABB находится вне фрустума.
            return false;
        }
        // Оптимизация: можно также проверять "отрицательную" вершину, чтобы определить полное нахождение внутри,
        // но для простого отсечения достаточно проверки положительной.
    }

    // Если AABB не был полностью отсечен ни одной из плоскостей, значит он видим (или пересекает фрустум)
    return true;
}

// *** КЛЮЧЕВЫЕ ИЗМЕНЕНИЯ: Проверка/Генерация меша по требованию ***
// Принимает НЕ-const ссылку на мир
void Renderer::renderWorld(World& world, const Camera& camera, Shader& blockShader, const std::array<Plane, 6>& frustumPlanes) const {
    blockShader.use();
    glm::mat4 projection = camera.getProjectionMatrix();
    glm::mat4 view = camera.getViewMatrix();
    blockShader.setMat4("projection", projection);
    blockShader.setMat4("view", view);

    int chunksRendered = 0;
    int chunksTotal = 0;
    // Получаем НЕ-const доступ, но используем как const внутри
    for (auto const& [chunkPos, chunkPtr] : world.getChunks()) { // Можно const&
        if (!chunkPtr) continue;
        chunksTotal++;

        // 1. Frustum Culling
        if (!isAABBInFrustum(chunkPtr->getAABB(), frustumPlanes)) {
            continue;
        }

        // 2. Получение и отрисовка меша (НЕ ГЕНЕРИРУЕМ ЗДЕСЬ)
        const Mesh* chunkMesh = chunkPtr->getMesh(); // Просто получаем меш
        if (chunkMesh && chunkMesh->isValid()) {
            glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(chunkPtr->getMinWorldPos()));
            blockShader.setMat4("model", model);
            chunkMesh->draw(); // Рисуем готовый меш
            chunksRendered++;
        }
    }

    // Отладочный вывод
    static float timeSincePrint = 0.0f; timeSincePrint += 0.016f;
    if (timeSincePrint > 1.0f) {
        std::cout << "Renderer: Rendered " << chunksRendered << " / " << chunksTotal << " chunk meshes this frame." << std::endl;
        timeSincePrint = 0.0f;
    }

    GLenum err; while ((err = glGetError()) != GL_NO_ERROR) { /*...*/ }
}





// --- renderGrid --- (Без изменений)
void Renderer::renderGrid(Shader& lineShader, GLuint gridVAO, GLsizei gridVertexCount, const glm::mat4& view, const glm::mat4& projection) const {
    if (gridVAO == 0 || gridVertexCount == 0) return;
    lineShader.use();
    lineShader.setMat4("projection", projection);
    lineShader.setMat4("view", view);
    lineShader.setMat4("model", glm::mat4(1.0f));
    glLineWidth(1.0f);
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);
}

// --- renderDebugInfo --- (Без изменений)
void Renderer::renderDebugInfo(const Camera& camera, Shader& lineShader, GLuint debugAxesVAO, GLsizei debugAxesVertexCount) const {
    if (debugAxesVAO == 0 || debugAxesVertexCount == 0) return;
    glDisable(GL_DEPTH_TEST); glLineWidth(3.0f);
    lineShader.use();
    glm::mat4 projection = camera.getProjectionMatrix(); glm::mat4 view = camera.getViewMatrix();
    lineShader.setMat4("projection", projection); lineShader.setMat4("view", view);
    float lineLength = 0.5f; float lineOffset = 1.5f;
    glm::vec3 lineOrigin = camera.Position + camera.Front * lineOffset;
    glBindVertexArray(debugAxesVAO);
    glm::mat4 modelAxisX = glm::scale(glm::translate(glm::mat4(1.0f), lineOrigin), glm::vec3(lineLength, 1.0f, 1.0f));
    lineShader.setMat4("model", modelAxisX); glDrawArrays(GL_LINES, 0, 2);
    glm::mat4 modelAxisY = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), lineOrigin), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)), glm::vec3(lineLength, 1.0f, 1.0f));
    lineShader.setMat4("model", modelAxisY); glDrawArrays(GL_LINES, 2, 2);
    glm::mat4 modelAxisZ = glm::scale(glm::rotate(glm::translate(glm::mat4(1.0f), lineOrigin), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)), glm::vec3(lineLength, 1.0f, 1.0f));
    lineShader.setMat4("model", modelAxisZ); glDrawArrays(GL_LINES, 4, 2);
    glBindVertexArray(0);
    glLineWidth(1.0f); glEnable(GL_DEPTH_TEST);
}