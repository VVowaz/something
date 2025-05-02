#pragma once

#include "Block.h"   // Для WorldDataStructure и BlockType
#include <string>
#include <vector>
#include <fstream>   // Для работы с файлами
#include <iostream> // Для ошибок

class World; // Прямое объявление

class WorldStorage {
public:
    WorldStorage(const std::string& worldDir = "worlds"); // Конструктор, задает папку
    ~WorldStorage();

    // Сохраняет текущее состояние мира в файл
    bool saveWorld(const World& world, const std::string& worldName) const;

    // Загружает данные мира из файла. Возвращает пустой вектор при ошибке.
    // Изменяет width, height, depth по ссылке.
    WorldDataStructure loadWorldData(const std::string& worldName, int& width, int& height, int& depth);
    bool saveWorldData(const WorldDataStructure& worldData,
        int width, int height, int depth,
        const std::string& worldName) const;
    // Проверяет, существует ли файл мира
    bool worldExists(const std::string& worldName) const;

private:
    std::string worldDirectory; // Папка для хранения миров

    // Вспомогательный метод для получения полного пути к файлу
    std::string getFilePath(const std::string& worldName) const;
};