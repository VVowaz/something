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