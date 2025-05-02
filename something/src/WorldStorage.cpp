#include "WorldStorage.h"
#include "World.h"  // Нужно для доступа к методам World при сохранении
#include "Chunk.h"  // Нужно для доступа к размерам чанка
#include <filesystem> // Для работы с путями и проверки существования файла (C++17)
#include <fstream>
#include <iostream>

// Требует C++17 для std::filesystem
namespace fs = std::filesystem;

WorldStorage::WorldStorage(const std::string& worldDir) : worldDirectory(worldDir) {
    // Создаем папку для миров, если она не существует
    try {
        if (!fs::exists(worldDirectory)) {
            if (fs::create_directory(worldDirectory)) {
                std::cout << "WorldStorage: Created world directory: " << worldDirectory << std::endl;
            }
            else {
                std::cerr << "ERROR::WORLDSTORAGE: Failed to create world directory: " << worldDirectory << std::endl;
            }
        }
        else if (!fs::is_directory(worldDirectory)) {
            std::cerr << "ERROR::WORLDSTORAGE: Path exists but is not a directory: " << worldDirectory << std::endl;
            // Можно бросить исключение или попытаться работать без директории
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "ERROR::WORLDSTORAGE: Filesystem error checking/creating directory: " << e.what() << std::endl;
    }
}

WorldStorage::~WorldStorage() {}

std::string WorldStorage::getFilePath(const std::string& worldName) const {
    // Добавляем расширение .world (или другое по желанию)
    return worldDirectory + "/" + worldName + ".world";
}

bool WorldStorage::worldExists(const std::string& worldName) const {
    return fs::exists(getFilePath(worldName));
}

bool WorldStorage::saveWorld(const World& world, const std::string& worldName) const {
    std::string filepath = getFilePath(worldName);
    // Открываем файл для бинарной записи, перезаписывая содержимое (trunc)
    std::ofstream outFile(filepath, std::ios::binary | std::ios::trunc);

    if (!outFile.is_open()) {
        std::cerr << "ERROR::WORLDSTORAGE::SAVE: Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    // 1. Получаем размеры мира
    int width = world.getWidth();
    int height = world.getHeight();
    int depth = world.getDepth();

    if (width <= 0 || height <= 0 || depth <= 0) {
        std::cerr << "ERROR::WORLDSTORAGE::SAVE: Invalid world dimensions (" << width << "," << height << "," << depth << "), cannot save." << std::endl;
        outFile.close();
        return false;
    }

    // 2. Записываем размеры в начало файла
    outFile.write(reinterpret_cast<const char*>(&width), sizeof(width));
    outFile.write(reinterpret_cast<const char*>(&height), sizeof(height));
    outFile.write(reinterpret_cast<const char*>(&depth), sizeof(depth));

    // 3. Записываем данные блоков (проходим по X, потом Z, потом Y для кеш-эффективности?)
    // Или проходим по чанкам, а внутри по блокам чанка
    std::cout << "WorldStorage: Saving world '" << worldName << "' (" << width << "x" << height << "x" << depth << ")..." << std::endl;
    size_t blocksWritten = 0;
    for (int x = 0; x < width; ++x) {
        for (int z = 0; z < depth; ++z) {
            // Можно оптимизировать, получая чанк один раз для столбца Z
            // const Chunk* currentChunk = world.getChunk(x, z); // Получаем указатель на чанк
            // if (!currentChunk) { /* Ошибка или пропуск? */ continue; }
            // glm::ivec3 localBase = world.worldToLocalCoords(x, 0, z);

            for (int y = 0; y < height; ++y) {
                // BlockType type = currentChunk->getBlock(localBase.x, y, localBase.z); // Получаем из чанка
                BlockType type = world.getBlockType(x, y, z); // Проще, но может быть медленнее
                // Записываем тип блока (обычно 1 байт, если enum class : uint8_t)
                outFile.write(reinterpret_cast<const char*>(&type), sizeof(BlockType));
                blocksWritten++;
            }
        }
        // Небольшой прогресс-индикатор
        if ((x + 1) % (width / 10 + 1) == 0) {
            std::cout << "Saving progress: " << static_cast<int>((static_cast<float>(x + 1) / width) * 100) << "%" << std::endl;
        }
    }

    outFile.close();

    if (outFile.good()) { // Проверяем, не было ли ошибок при записи/закрытии
        std::cout << "WorldStorage: World '" << worldName << "' saved successfully (" << blocksWritten << " blocks)." << std::endl;
        return true;
    }
    else {
        std::cerr << "ERROR::WORLDSTORAGE::SAVE: Error occurred during file write/close: " << filepath << std::endl;
        return false;
    }
}

WorldDataStructure WorldStorage::loadWorldData(const std::string& worldName, int& width, int& height, int& depth) {
    std::string filepath = getFilePath(worldName);
    width = 0; height = 0; depth = 0; // Сбрасываем размеры
    WorldDataStructure loadedData; // Пустой вектор по умолчанию

    if (!worldExists(worldName)) {
        std::cout << "WorldStorage: World file not found: " << filepath << std::endl;
        return loadedData; // Возвращаем пустой вектор
    }

    std::ifstream inFile(filepath, std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "ERROR::WORLDSTORAGE::LOAD: Failed to open file for reading: " << filepath << std::endl;
        return loadedData;
    }

    std::cout << "WorldStorage: Loading world '" << worldName << "'..." << std::endl;

    // 1. Читаем размеры
    inFile.read(reinterpret_cast<char*>(&width), sizeof(width));
    inFile.read(reinterpret_cast<char*>(&height), sizeof(height));
    inFile.read(reinterpret_cast<char*>(&depth), sizeof(depth));

    if (inFile.fail() || width <= 0 || height <= 0 || depth <= 0) {
        std::cerr << "ERROR::WORLDSTORAGE::LOAD: Invalid dimensions read from file: (" << width << "," << height << "," << depth << ")" << std::endl;
        inFile.close();
        width = height = depth = 0; // Сбрасываем размеры
        return loadedData; // Возвращаем пустой вектор
    }

    // 2. Подготавливаем структуру данных нужного размера
    try {
        loadedData.resize(width, std::vector<std::vector<BlockType>>(height, std::vector<BlockType>(depth)));
    }
    catch (const std::bad_alloc& e) {
        std::cerr << "ERROR::WORLDSTORAGE::LOAD: Failed to allocate memory for world data: " << e.what() << std::endl;
        inFile.close();
        width = height = depth = 0;
        return WorldDataStructure(); // Возвращаем пустой вектор
    }

    // 3. Читаем данные блоков
    size_t blocksRead = 0;
    size_t totalBlocks = static_cast<size_t>(width) * height * depth;
    for (int x = 0; x < width; ++x) {
        for (int z = 0; z < depth; ++z) {
            for (int y = 0; y < height; ++y) {
                BlockType type;
                inFile.read(reinterpret_cast<char*>(&type), sizeof(BlockType));
                if (inFile.fail()) {
                    std::cerr << "ERROR::WORLDSTORAGE::LOAD: Failed to read block data at (" << x << "," << y << "," << z << "). File might be corrupted." << std::endl;
                    inFile.close();
                    width = height = depth = 0;
                    return WorldDataStructure(); // Возвращаем пустой
                }
                loadedData[x][y][z] = type;
                blocksRead++;
            }
        }
    }

    inFile.close();

    if (blocksRead != totalBlocks) {
        std::cerr << "ERROR::WORLDSTORAGE::LOAD: Read " << blocksRead << " blocks, expected " << totalBlocks << ". File might be incomplete." << std::endl;
        width = height = depth = 0;
        return WorldDataStructure(); // Возвращаем пустой
    }

    std::cout << "WorldStorage: World '" << worldName << "' loaded successfully (" << blocksRead << " blocks)." << std::endl;
    return loadedData; // Возвращаем загруженные данные
}

bool WorldStorage::saveWorldData(const WorldDataStructure& worldData,
    int width, int height, int depth,
    const std::string& worldName) const
{
    std::string filepath = getFilePath(worldName);
    // Открываем для бинарной записи с перезаписью
    std::ofstream outFile(filepath, std::ios::binary | std::ios::trunc);

    if (!outFile.is_open()) {
        std::cerr << "ERROR::WORLDSTORAGE::SAVEDATA: Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    // 1. Проверка корректности переданных размеров и данных
    if (width <= 0 || height <= 0 || depth <= 0 ||
        worldData.empty() || worldData.size() != width ||
        worldData[0].empty() || worldData[0].size() != height ||
        worldData[0][0].empty() || worldData[0][0].size() != depth)
    {
        std::cerr << "ERROR::WORLDSTORAGE::SAVEDATA: Invalid dimensions or data provided for saving."
            << " Expected (" << width << "," << height << "," << depth << ")"
            << ", Data size (" << worldData.size() << ", ...)" << std::endl;
        outFile.close();
        return false;
    }

    // 2. Записываем размеры
    outFile.write(reinterpret_cast<const char*>(&width), sizeof(width));
    outFile.write(reinterpret_cast<const char*>(&height), sizeof(height));
    outFile.write(reinterpret_cast<const char*>(&depth), sizeof(depth));

    // 3. Записываем данные блоков
    std::cout << "WorldStorage: Saving world data '" << worldName << "' (" << width << "x" << height << "x" << depth << ")..." << std::endl;
    size_t blocksWritten = 0;
    size_t totalBlocks = static_cast<size_t>(width) * height * depth;
    for (int x = 0; x < width; ++x) {
        for (int z = 0; z < depth; ++z) { // Порядок Z/Y может влиять на кеш, но для простоты оставим Z внешним
            for (int y = 0; y < height; ++y) {
                // Проверка индексов перед доступом (на всякий случай, хотя размеры проверены)
                if (x >= worldData.size() || y >= worldData[x].size() || z >= worldData[x][y].size()) {
                    std::cerr << "ERROR::WORLDSTORAGE::SAVEDATA: Index out of bounds during write at (" << x << "," << y << "," << z << ")" << std::endl;
                    // Можно записать Air или прервать сохранение
                    BlockType air = BlockType::Air;
                    outFile.write(reinterpret_cast<const char*>(&air), sizeof(BlockType));
                }
                else {
                    BlockType type = worldData[x][y][z]; // Берем из переданной структуры
                    outFile.write(reinterpret_cast<const char*>(&type), sizeof(BlockType));
                }
                blocksWritten++;
            }
        }
        // Прогресс
        if ((x + 1) % (width / 10 + 1) == 0) {
            std::cout << "Saving progress: " << static_cast<int>((static_cast<float>(x + 1) / width) * 100) << "%" << std::endl;
        }
    }

    outFile.close();

    if (outFile.good() && blocksWritten == totalBlocks) {
        std::cout << "WorldStorage: World data '" << worldName << "' saved successfully (" << blocksWritten << " blocks)." << std::endl;
        return true;
    }
    else {
        std::cerr << "ERROR::WORLDSTORAGE::SAVEDATA: Error during file write/close or block count mismatch. Written: " << blocksWritten << ", Expected: " << totalBlocks << std::endl;
        // Можно попытаться удалить некорректный файл
        // fs::remove(filepath);
        return false;
    }
}