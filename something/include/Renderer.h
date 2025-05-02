#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <array>
#include "Camera.h"
#include "Chunk.h"

class World; class Shader; class Mesh; // Прямые объявления

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete; Renderer& operator=(const Renderer&) = delete;

    void prepareFrame() const;
    // *** ИЗМЕНЕНО: cubeMesh больше не нужен ***
    void renderWorld(World& world, const Camera& camera, Shader& blockShader, const std::array<Plane, 6>& frustumPlanes) const;
    void renderGrid(Shader& lineShader, GLuint gridVAO, GLsizei gridVertexCount, const glm::mat4& view, const glm::mat4& projection) const;
    void renderDebugInfo(const Camera& camera, Shader& lineShader, GLuint debugAxesVAO, GLsizei debugAxesVertexCount) const;

    bool isAABBInFrustum(const AABB& box, const std::array<Plane, 6>& planes) const;
};