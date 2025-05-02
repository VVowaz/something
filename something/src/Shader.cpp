#include "Shader.h"
#include "Utils.h" // For readFile
#include <glm/gtc/type_ptr.hpp>
#include <iostream>  // For std::cerr/std::endl
#include <fstream>
#include <sstream>
#include <string>

Shader::Shader(const char* vertexPath, const char* fragmentPath) : ID(0) {
    // 1. Retrieve shader source code from files
    std::string vertexCodeStr = readFile(vertexPath);
    std::string fragmentCodeStr = readFile(fragmentPath);

    if (vertexCodeStr.empty() || fragmentCodeStr.empty()) {
        std::cerr << "ERROR::SHADER::CONSTRUCTOR: Failed to read one or both shader files." << std::endl;
        // ID is already 0, constructor will indicate failure
        return;
    }

    const char* vShaderCode = vertexCodeStr.c_str();
    const char* fShaderCode = fragmentCodeStr.c_str();

    // 2. Compile shaders
    GLuint vertex, fragment;

    // Vertex Shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    if (!checkCompileErrors(vertex, "VERTEX")) {
        glDeleteShader(vertex);
        return; // ID remains 0
    }

    // Fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    if (!checkCompileErrors(fragment, "FRAGMENT")) {
        glDeleteShader(vertex); // Clean up previous one
        glDeleteShader(fragment);
        return; // ID remains 0
    }

    // 3. Link shader program
    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    if (!checkLinkErrors(ID)) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        glDeleteProgram(ID);
        ID = 0; // Indicate failure
        return;
    }

    // Shaders are linked, no longer needed individually
    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader() {
    if (ID != 0) {
        glDeleteProgram(ID);
    }
}

void Shader::use() {
    if (ID != 0) {
        glUseProgram(ID);
    }
}

// --- Uniform Setters (no change needed) ---
void Shader::setBool(const std::string& name, bool value) const {
    if (ID != 0) glUniform1i(glGetUniformLocation(ID, name.c_str()), (int)value);
}
void Shader::setInt(const std::string& name, int value) const {
    if (ID != 0) glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}
void Shader::setFloat(const std::string& name, float value) const {
    if (ID != 0) glUniform1f(glGetUniformLocation(ID, name.c_str()), value);
}
void Shader::setVec3(const std::string& name, const glm::vec3& value) const {
    if (ID != 0) glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(value));
}
void Shader::setVec3(const std::string& name, float x, float y, float z) const {
    if (ID != 0) glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z);
}
void Shader::setMat4(const std::string& name, const glm::mat4& mat) const {
    if (ID != 0) glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}


// --- Error Checking ---
bool Shader::checkCompileErrors(GLuint shader, const std::string& type) {
    GLint success;
    GLchar infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, NULL, infoLog);
        // English error message
        std::cerr << "ERROR::SHADER::COMPILATION::" << type << "\n" << infoLog << std::endl;
        return false;
    }
    return true;
}

bool Shader::checkLinkErrors(GLuint program) {
    GLint success;
    GLchar infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 1024, NULL, infoLog);
        // English error message
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        return false;
    }
    return true;
}