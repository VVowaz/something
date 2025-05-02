#include "WorldLoader.h"
#include "WorldStorage.h"   // Включаем для работы с файлами
#include "WorldGenerator.h" // Включаем для генерации нового мира
#include "World.h"          // Включаем для создания объекта World
#include "Block.h"          // Включаем для WorldDataStructure
#include <iostream>
#include <memory>           // Для std::make_unique
#include <utility>          // Для std::move
#include <functional>       // Для std::function (SurfaceHeightFunction)
#include <cmath>            // Для sin/cos в генерации по умолчанию

// --- Конструктор ---
WorldLoader::WorldLoader(const std::string& name, const std::string& storageDir)
    : worldName(name)
{
    // Создаем объект хранилища при инициализации
    storage = std::make_unique<WorldStorage>(storageDir);
    if (!storage) {
        // Обработка ошибки, если не удалось создать хранилище
        std::cerr << "FATAL::WORLDLOADER: Failed to create WorldStorage!" << std::endl;
        // Можно бросить исключение
        throw std::runtime_error("Failed to create WorldStorage.");
    }
    std::cout << "WorldLoader initialized for world: '" << worldName << "' in directory: '" << storageDir << "'" << std::endl;
}

// --- Деструктор ---
WorldLoader::~WorldLoader() {
    // unique_ptr storage удалится автоматически
}

// --- Основной метод загрузки/создания ---
std::unique_ptr<World> WorldLoader::loadOrCreateWorld() {
    WorldDataStructure worldData; // Здесь будут данные
    bool needsGeneration = false;

    // 1. Пытаемся загрузить мир
    if (storage->worldExists(worldName)) {
        std::cout << "WorldLoader: Loading existing world '" << worldName << "'..." << std::endl;
        worldData = storage->loadWorldData(worldName, worldWidth, worldHeight, worldDepth);

        // Проверяем результат загрузки
        if (worldWidth <= 0 || worldHeight <= 0 || worldDepth <= 0 || worldData.empty()) {
            std::cerr << "WorldLoader: Failed to load valid data from existing world file. Will generate a new world." << std::endl;
            needsGeneration = true;
            // Устанавливаем размеры по умолчанию для генерации
            worldWidth = 64; worldHeight = 50; worldDepth = 64;
        }
        else {
            std::cout << "WorldLoader: World loaded successfully. Dimensions: "
                << worldWidth << "x" << worldHeight << "x" << worldDepth << std::endl;
        }
    }
    else {
        // Файл не найден, нужно генерировать
        std::cout << "WorldLoader: World file not found. Will generate a new world '" << worldName << "'." << std::endl;
        needsGeneration = true;
        // Устанавливаем размеры по умолчанию
        worldWidth = 64; worldHeight = 50; worldDepth = 64;
    }

    // 2. Генерируем данные, если нужно
    if (needsGeneration) {
        std::cout << "WorldLoader: Generating world data..." << std::endl;
        worldData = generateNewWorldData(); // Вызываем вспомогательный метод
        if (worldData.empty()) {
            std::cerr << "FATAL::WORLDLOADER: World generation failed to produce data." << std::endl;
            return nullptr; // Критическая ошибка
        }
        // Сохраняем только что сгенерированные данные
        if (!saveGeneratedData(worldData)) {
            std::cerr << "Warning::WORLDLOADER: Failed to save newly generated world data." << std::endl;
            // Продолжаем работу, но мир не будет сохранен
        }
    }

    // 3. Создаем и заполняем объект World
    std::unique_ptr<World> finalWorld = nullptr;
    try {
        std::cout << "WorldLoader: Creating and populating World object..." << std::endl;
        // Создаем мир с определенными размерами
        finalWorld = std::make_unique<World>(worldWidth, worldHeight, worldDepth);
        // Заполняем его данными и генерируем меши
        finalWorld->populate(std::move(worldData)); // Перемещаем данные
        std::cout << "WorldLoader: World object ready." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "FATAL::WORLDLOADER: Exception during World object creation/population: " << e.what() << std::endl;
        finalWorld.reset(); // Обнуляем указатель в случае ошибки
    }

    return finalWorld; // Возвращаем unique_ptr (может быть nullptr при ошибке)
}

// --- Вспомогательный метод генерации ---
WorldDataStructure WorldLoader::generateNewWorldData() {
    // Используем те же параметры генерации, что были в Application
    const int baseHeight = 28;
    const float frequency = 0.05f;
    const float amplitude = 10.0f;
    const float freqZMultiplier = 0.8f;
    const float amplZMultiplier = 0.7f;

    // Функция поверхности (синус)
    WorldGenerator::SurfaceHeightFunction surfaceFunc =
        [=](int x, int z) -> int {
        float waveX = std::sin(static_cast<float>(x) * frequency) * amplitude;
        float waveZ = std::cos(static_cast<float>(z) * frequency * freqZMultiplier) * amplitude * amplZMultiplier;
        int calculatedHeight = baseHeight + static_cast<int>(waveX + waveZ);
        // Ограничение высоты (важно!)
        if (calculatedHeight < 0) calculatedHeight = 0;
        // Сверяем с высотой мира, сохраненной в WorldLoader
        if (calculatedHeight >= this->worldHeight) calculatedHeight = this->worldHeight - 1;
        return calculatedHeight;
        };

    // Создаем генератор с текущими размерами мира
    WorldGenerator generator(worldWidth, worldHeight, worldDepth, surfaceFunc);

    // Генерируем и возвращаем данные
    WorldDataStructure data;
    if (generator.fillWorldData(data)) {
        return data;
    } else {
        return WorldDataStructure(); // Возвращаем пустой в случае ошибки
    }
}

// --- Вспомогательный метод сохранения ---
bool WorldLoader::saveGeneratedData(const WorldDataStructure& data) {
    if (!storage) {
        std::cerr << "ERROR::WORLDLOADER: WorldStorage not initialized, cannot save." << std::endl;
        return false;
    }
    std::cout << "WorldLoader: Saving generated world data for '" << worldName << "'..." << std::endl;
    // Используем новый метод сохранения в WorldStorage
    return storage->saveWorldData(data, worldWidth, worldHeight, worldDepth, worldName);
}


// --- Метод сохранения мира ---
bool WorldLoader::saveWorld(const World& world) {
    if (!storage) {
        std::cerr << "ERROR::WORLDLOADER: WorldStorage not initialized, cannot save." << std::endl;
        return false;
    }
    std::cout << "WorldLoader: Saving world '" << worldName << "'..." << std::endl;
    return storage->saveWorld(world, worldName); // Вызываем метод хранилища
}