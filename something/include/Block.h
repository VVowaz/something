#pragma once
#include <vector> // <<<--- Добавляем для std::vector

// Перечисление для различных типов блоков
enum class BlockType {
    Air,
    Grass,
    Stone,
};

// *** НОВОЕ: Определяем общий тип данных для мира ***
// (3D вектор типов блоков)
using WorldDataStructure = std::vector<std::vector<std::vector<BlockType>>>;


// Вспомогательные функции (без изменений)
inline bool isBlockVisible(BlockType type) {
    return type != BlockType::Air;
}
inline bool isBlockTransparent(BlockType type) {
    return type == BlockType::Air;
}