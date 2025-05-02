#include "WorldGenerator.h"
#include <stdexcept>
#include <iostream>

// Конструктор (без изменений)
WorldGenerator::WorldGenerator(int width, int height, int depth, SurfaceHeightFunction surfaceFunc)
    : width(width), height(height), depth(depth), surfaceHeightFunction(surfaceFunc)
{
    if (width <= 0 || height <= 0 || depth <= 0) { /* ... */ }
    // if (!surfaceHeightFunction) { /* ... */ } // Не критично для текущего заполнения
    std::cout << "WorldGenerator created for size " << width << "x" << height << "x" << depth << "." << std::endl;
}

// *** ИЗМЕНЕНО: Реализация fillWorldData ***
bool WorldGenerator::fillWorldData(WorldDataType& dataToFill) {
    std::cout << "WorldGenerator: Filling world data structure..." << std::endl;

    // 1. Проверяем/изменяем размер переданного вектора
    try {
        dataToFill.resize(width, std::vector<std::vector<BlockType>>(height, std::vector<BlockType>(depth, BlockType::Air)));
        std::cout << "WorldGenerator: Resized data structure to " << width << "x" << height << "x" << depth << "." << std::endl;
    }
    catch (const std::bad_alloc& e) {
        std::cerr << "ERROR::WORLDGENERATOR: Failed to allocate memory for world data: " << e.what() << std::endl;
        return false; // Не удалось выделить память
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR::WORLDGENERATOR: Exception during data resize: " << e.what() << std::endl;
        return false;
    }
    if (dataToFill.size() != width || dataToFill[0].size() != height || dataToFill[0][0].size() != depth) {
        std::cerr << "FATAL::GENERATOR: dataToFill has incorrect dimensions after resize!" << std::endl;
        return false;
    }
    std::cout << "DEBUG::GENERATOR: dataToFill resized correctly." << std::endl;


    // Цикл генерации с использованием surfaceHeightFunction
    for (int x = 0; x < width; ++x) {
        // Проверка внешнего индекса X
        if (x < 0 || x >= dataToFill.size()) {
            std::cerr << "FATAL::GENERATOR: Index X out of bounds! x=" << x << std::endl; continue;
        }
        for (int z = 0; z < depth; ++z) {
            // Проверка внутреннего индекса Z
            if (z < 0 || z >= dataToFill[x][0].size()) { // Проверяем по dataToFill[x][0]
                std::cerr << "FATAL::GENERATOR: Index Z out of bounds! z=" << z << std::endl; continue;
            }

            int surfaceY = surfaceHeightFunction(x, z);
            if (surfaceY < 0) surfaceY = 0;
            if (surfaceY >= height) surfaceY = height - 1; // Важно! >= height

            for (int y = 0; y < height; ++y) {
                // Проверка внутреннего индекса Y
                if (y < 0 || y >= dataToFill[x].size()) {
                    std::cerr << "FATAL::GENERATOR: Index Y out of bounds! y=" << y << std::endl; continue;
                }

                // *** ВОЗМОЖНОЕ МЕСТО ПОВРЕЖДЕНИЯ ПАМЯТИ ***
                BlockType blockToSet;
                if (y < surfaceY)       blockToSet = BlockType::Stone;
                else if (y == surfaceY) blockToSet = BlockType::Grass;
                else                    blockToSet = BlockType::Air;

                // Запись в вектор
                dataToFill[x][y][z] = blockToSet; // <<<--- Попытка записи
            }
        }
    }
    std::cout << "WorldGenerator: World data structure filled." << std::endl;
    return true;
}
// *** КОНЕЦ ИЗМЕНЕНИЙ ***

// *** НОВЫЙ МЕТОД: Реализация getBlockTypeAt ***
BlockType WorldGenerator::getBlockTypeAt(int worldX, int worldY, int worldZ) const {
    // Проверка выхода за границы генерации
    if (worldX < 0 || worldX >= width ||
        worldY < 0 || worldY >= height ||
        worldZ < 0 || worldZ >= depth)
    {
        return BlockType::Air; // Считаем воздух за пределами
    }

    // Вычисляем высоту поверхности для данного столба X/Z
    int surfaceY = surfaceHeightFunction(worldX, worldZ);
    // Ограничиваем (на всякий случай, хотя координаты Y проверяются выше)
    if (surfaceY < 0) surfaceY = 0;
    if (surfaceY >= height) surfaceY = height - 1;

    // Определяем тип блока на основе Y и высоты поверхности
    if (worldY < surfaceY) {
        return BlockType::Stone;
    }
    else if (worldY == surfaceY) {
        return BlockType::Grass;
    }
    else {
        return BlockType::Air;
    }
}

