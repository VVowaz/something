#pragma once

#include <string>
#include <memory>   // Для std::unique_ptr
#include "Block.h"   // Включаем для WorldDataStructure
#include <vector>   // Убедимся, что vector включен (для WorldDataStructure)

// Прямые объявления для уменьшения зависимостей
class World;
class WorldStorage;
class WorldGenerator;

// Класс, отвечающий за координацию загрузки существующего мира
// или генерацию нового мира.
class WorldLoader {
public:
    // Конструктор: принимает имя мира и путь к папке хранилища
    WorldLoader(const std::string& worldName = "myworld", const std::string& storageDir = "worlds");
    ~WorldLoader(); // Простой деструктор

    // Запрещаем копирование и присваивание
    WorldLoader(const WorldLoader&) = delete;
    WorldLoader& operator=(const WorldLoader&) = delete;

    // Основной метод: пытается загрузить мир, если не удается - генерирует новый.
    // Возвращает unique_ptr на готовый к использованию объект World (с чанками и мешами)
    // или nullptr в случае критической ошибки.
    std::unique_ptr<World> loadOrCreateWorld();

    // Сохраняет переданный мир (вызывает WorldStorage)
    bool saveWorld(const World& world);

private:
    std::string worldName; // Имя текущего мира
    std::unique_ptr<WorldStorage> storage; // Указатель на объект хранилища

    // Размеры мира, определенные при загрузке или установленные для нового
    int worldWidth = 0;
    int worldHeight = 0;
    int worldDepth = 0;

    // Вспомогательный метод для генерации данных нового мира
    WorldDataStructure generateNewWorldData();

    // Вспомогательный метод для сохранения данных (используется при генерации)
    bool saveGeneratedData(const WorldDataStructure& data);
};