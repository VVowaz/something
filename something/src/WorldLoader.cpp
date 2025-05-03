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

// --- Основной метод загрузки/создания (Работает с Дельтой) ---
std::unique_ptr<World> WorldLoader::loadOrCreateWorld() {
    // Размеры мира (пока фиксированные, можно вынести в конфиг или метаданные мира)
    // Важно: Размеры должны быть кратны размерам чанка!
    worldWidth = 64;  // Например, 4 чанка по X
    worldHeight = 50;
    worldDepth = 64;  // Например, 4 чанка по Z

    // 1. Загружаем Дельту (если файл существует)
    DeltaStorage loadedDelta; // Создаем пустую дельту
    if (storage->worldExists(worldName)) { // worldExists проверяет файл .delta
        std::cout << "WorldLoader: Loading delta for existing world '" << worldName << "'..." << std::endl;
        loadedDelta = storage->loadDelta(worldName); // loadDelta возвращает DeltaStorage
        std::cout << "WorldLoader: Delta loaded with " << loadedDelta.getChangeCount() << " changes." << std::endl;
    }
    else {
        std::cout << "WorldLoader: Delta file not found. Creating new world '" << worldName << "' with empty delta." << std::endl;
        // loadedDelta остается пустой
    }

    // 2. Создаем Функцию Генерации Поверхности
    // (Можно вынести в отдельный метод или класс для выбора генератора)
    std::cout << "WorldLoader: Setting up world generation function (Sine Hills)..." << std::endl;
    const int baseHeight = 28; const float frequency = 0.05f; const float amplitude = 10.0f;
    const float freqZMultiplier = 0.8f; const float amplZMultiplier = 0.7f;
    // Захватываем высоту мира для ограничения
    int currentWorldHeight = this->worldHeight;

    World::SurfaceHeightFunction generationFunc = // Используем тип из World
        [=](int x, int z) -> int {
        float waveX = std::sin(static_cast<float>(x) * frequency) * amplitude;
        float waveZ = std::cos(static_cast<float>(z) * frequency * freqZMultiplier) * amplitude * amplZMultiplier;
        int calculatedHeight = baseHeight + static_cast<int>(waveX + waveZ);
        if (calculatedHeight < 0) calculatedHeight = 0;
        // Используем захваченную высоту
        if (calculatedHeight >= currentWorldHeight) calculatedHeight = currentWorldHeight - 1;
        return calculatedHeight;
        };
    // --- Функция готова ---

    // --- Создаем объект World ---
    std::unique_ptr<World> finalWorld = nullptr;
    try {
        std::cout << "WorldLoader: Creating World object..." << std::endl;
        // Передаем размеры, ФУНКЦИЮ ГЕНЕРАЦИИ и дельту
        finalWorld = std::make_unique<World>(worldWidth, worldHeight, worldDepth,
            generationFunc, std::move(loadedDelta)); // <<<--- Передаем функцию
        std::cout << "WorldLoader: World object created." << std::endl;
        // populateAndGenerateMeshes больше не нужен
    }
    catch (const std::exception& e) { /* ... обработка ошибки ... */ }

    return finalWorld;
}

// --- Метод сохранения мира (Сохраняет Дельту) ---
bool WorldLoader::saveWorld(const World& world) {
    if (!storage) {
        std::cerr << "ERROR::WORLDLOADER: WorldStorage not initialized, cannot save." << std::endl;
        return false;
    }
    std::cout << "WorldLoader: Saving world delta for '" << worldName << "'..." << std::endl;

    // Получаем дельту из мира
    const DeltaStorage& deltaToSave = world.getDeltaStorage(); // Нужен этот геттер в World

    // Вызываем метод сохранения дельты в хранилище
    return storage->saveDelta(deltaToSave, worldName);
}

// --- Вспомогательные методы generateNewWorldData и saveGeneratedData больше не нужны ---
// WorldDataStructure WorldLoader::generateNewWorldData() { /* ... */ }
// bool WorldLoader::saveGeneratedData(const WorldDataStructure& data) { /* ... */ }