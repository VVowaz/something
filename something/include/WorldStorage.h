#pragma once

#include "Block.h"   // Для WorldDataStructure и BlockType
#include <string>
#include <vector>
#include <fstream>   // Для работы с файлами
#include <iostream> // Для ошибок

class World; // Прямое объявление
class DeltaStorage;

class WorldStorage {
public:
    WorldStorage(const std::string& worldDir = "worlds"); // Конструктор, задает папку
    ~WorldStorage();

    // Сохраняет текущее состояние мира в файл
    bool saveDelta(const DeltaStorage& delta, const std::string& worldName) const;

    // Загружает данные мира из файла. Возвращает пустой вектор при ошибке.
    // Изменяет width, height, depth по ссылке.
    DeltaStorage loadDelta(const std::string& worldName);
    bool saveWorldData(const WorldDataStructure& worldData,
        int width, int height, int depth,
        const std::string& worldName) const;
    // Проверяет, существует ли файл мира
    bool worldExists(const std::string& worldName) const;

private:
    std::string worldDirectory; // Папка для хранения миров

    // Вспомогательный метод для получения полного пути к файлу
    std::string getFilePath(const std::string& worldName) const;
    const uint32_t DELTA_MAGIC_NUMBER = 0x574F5244; // "WORD" в ASCII (Little Endian)
    const uint16_t DELTA_FILE_VERSION = 1;
};