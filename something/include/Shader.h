#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string> // <<--- Убедимся, что есть

class Shader {
public:
    GLuint ID; // ID программы

    Shader(const char* vertexPath, const char* fragmentPath);
    ~Shader();

    // Запрещаем копирование
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    // Можно добавить перемещение, если нужно будет хранить шейдеры в контейнерах,
    // но пока оставим так для простоты

    void use();
    // Используем const std::string&
    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

private:
    // Используем const std::string&
    bool checkCompileErrors(GLuint shader, const std::string& type);
    bool checkLinkErrors(GLuint program);
};