#pragma once

#include <GL/glew.h>
#include <vector>
#include <glm/glm.hpp>

// Структура для представления вершины (для удобства)
// Позже можно вынести в отдельный файл
struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

// Класс для хранения и отрисовки геометрии.
// Теперь работает с новым форматом Vertex.
class Mesh {
public:
    // Конструктор: принимает векторы вершин и индексов
    Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw() const;
    bool isValid() const { return VAO != 0 && VBO != 0 && EBO != 0 && indexCount > 0; }

private:
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLsizei indexCount = 0;

    // Внутренний метод для настройки буферов
    void setupMesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices);
    void cleanup();
};