#pragma once

#include "Block.h"    // Включаем для BlockType и WorldDataStructure!
// #include <vector> // Больше не нужно здесь, т.к. есть в Block.h
#include <functional>

class WorldGenerator {
public:
    using SurfaceHeightFunction = std::function<int(int, int)>;
    // Используем общее имя типа
    using WorldDataType = WorldDataStructure; // <<<--- Используем псевдоним

    WorldGenerator(int width, int height, int depth, SurfaceHeightFunction surfaceFunc);
    // *** ИЗМЕНЕНО: Метод теперь заполняет переданный контейнер ***
    // Принимает неконстантную ссылку на структуру данных
    bool fillWorldData(WorldDataType& dataToFill);

    // *** НОВЫЙ МЕТОД: Получить тип блока для конкретной координаты ***
    BlockType getBlockTypeAt(int worldX, int worldY, int worldZ) const;

private:
    int width, height, depth;
    SurfaceHeightFunction surfaceHeightFunction;
};