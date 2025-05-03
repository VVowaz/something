#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream> // For std::cerr

// Simple function to read a file into a string
inline std::string readFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        // Use std::cerr for errors
        std::cerr << "ERROR::UTILS::READ_FILE: Failed to open file: " << filepath << std::endl;
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    return buffer.str();
}

// Функтор сравнения для использования glm::ivec3 в качестве ключа std::map
struct ivec3_less {
    bool operator()(const glm::ivec3& a, const glm::ivec3& b) const {
        // Сравниваем последовательно по компонентам X, Y, Z
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    }
};