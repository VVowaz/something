#include "WorldStorage.h"
#include "World.h"  // Нужно для доступа к методам World при сохранении
#include "Chunk.h"  // Нужно для доступа к размерам чанка
#include "DeltaStorage.h"
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

bool WorldStorage::saveDelta(const DeltaStorage& delta, const std::string& worldName) const {
    std::string filepath = getFilePath(worldName);
    std::ofstream outFile(filepath, std::ios::binary | std::ios::trunc);

    if (!outFile.is_open()) {
        std::cerr << "ERROR::WORLDSTORAGE::SAVEDELTA: Failed to open file for writing: " << filepath << std::endl;
        return false;
    }

    std::cout << "WorldStorage: Saving delta for world '" << worldName << "'..." << std::endl;

    try {
        // 1. Записываем магическое число и версию
        outFile.write(reinterpret_cast<const char*>(&DELTA_MAGIC_NUMBER), sizeof(DELTA_MAGIC_NUMBER));
        outFile.write(reinterpret_cast<const char*>(&DELTA_FILE_VERSION), sizeof(DELTA_FILE_VERSION));

        // 2. Получаем карту изменений и записываем ее размер
        const auto& changes = delta.getAllChanges();
        size_t changeCount = changes.size();
        outFile.write(reinterpret_cast<const char*>(&changeCount), sizeof(changeCount));

        std::cout << "WorldStorage: Writing " << changeCount << " changes..." << std::endl;

        // 3. Записываем каждую пару ключ-значение (координаты и тип блока)
        for (const auto& [pos, type] : changes) {
            // Записываем координаты (3 * int)
            outFile.write(reinterpret_cast<const char*>(&pos.x), sizeof(pos.x));
            outFile.write(reinterpret_cast<const char*>(&pos.y), sizeof(pos.y));
            outFile.write(reinterpret_cast<const char*>(&pos.z), sizeof(pos.z));
            // Записываем тип блока (enum class, обычно sizeof(int) или sizeof(underlying_type))
            // Приведем к базовому типу для надежности (например, uint8_t)
            auto underlyingType = static_cast<std::underlying_type_t<BlockType>>(type);
            outFile.write(reinterpret_cast<const char*>(&underlyingType), sizeof(underlyingType));

            // Проверяем ошибки записи после каждой итерации (опционально, но надежно)
            if (outFile.fail()) {
                std::cerr << "ERROR::WORLDSTORAGE::SAVEDELTA: Failed to write change for block at ("
                    << pos.x << "," << pos.y << "," << pos.z << ")" << std::endl;
                outFile.close();
                return false;
            }
        }

    }
    catch (const std::exception& e) {
        std::cerr << "ERROR::WORLDSTORAGE::SAVEDELTA: Exception during saving: " << e.what() << std::endl;
        outFile.close();
        return false;
    }

    outFile.close();

    if (outFile.good()) {
        std::cout << "WorldStorage: Delta for world '" << worldName << "' saved successfully." << std::endl;
        return true;
    }
    else {
        std::cerr << "ERROR::WORLDSTORAGE::SAVEDELTA: Error occurred during file write/close: " << filepath << std::endl;
        return false;
    }
}


// *** НОВАЯ РЕАЛИЗАЦИЯ: loadDelta ***
DeltaStorage WorldStorage::loadDelta(const std::string& worldName) {
    std::string filepath = getFilePath(worldName);
    DeltaStorage loadedDelta; // Создаем пустой объект дельты

    if (!worldExists(worldName)) {
        std::cout << "WorldStorage: Delta file not found: " << filepath << ". Returning empty delta." << std::endl;
        return loadedDelta; // Возвращаем пустую дельту
    }

    std::ifstream inFile(filepath, std::ios::binary);
    if (!inFile.is_open()) {
        std::cerr << "ERROR::WORLDSTORAGE::LOADDELTA: Failed to open file for reading: " << filepath << std::endl;
        return loadedDelta; // Возвращаем пустую дельту
    }

    std::cout << "WorldStorage: Loading delta for world '" << worldName << "'..." << std::endl;

    try {
        // 1. Читаем и проверяем магическое число и версию
        uint32_t magic;
        uint16_t version;
        inFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        inFile.read(reinterpret_cast<char*>(&version), sizeof(version));

        if (inFile.fail() || magic != DELTA_MAGIC_NUMBER) {
            std::cerr << "ERROR::WORLDSTORAGE::LOADDELTA: Invalid magic number in file: " << filepath << std::endl;
            inFile.close();
            return loadedDelta;
        }
        if (version != DELTA_FILE_VERSION) {
            std::cerr << "Warning::WORLDSTORAGE::LOADDELTA: Mismatched file version (File: " << version
                << ", Expected: " << DELTA_FILE_VERSION << ") in: " << filepath << ". Attempting to load anyway." << std::endl;
            // Можно добавить обработку старых версий здесь
        }

        // 2. Читаем количество изменений
        size_t changeCount = 0;
        inFile.read(reinterpret_cast<char*>(&changeCount), sizeof(changeCount));
        if (inFile.fail()) {
            std::cerr << "ERROR::WORLDSTORAGE::LOADDELTA: Failed to read change count from file: " << filepath << std::endl;
            inFile.close();
            return loadedDelta;
        }

        std::cout << "WorldStorage: Reading " << changeCount << " changes..." << std::endl;

        // 3. Читаем каждую запись изменения
        for (size_t i = 0; i < changeCount; ++i) {
            glm::ivec3 pos;
            BlockType type;
            // Используем базовый тип для чтения
            std::underlying_type_t<BlockType> underlyingType;

            // Читаем координаты
            inFile.read(reinterpret_cast<char*>(&pos.x), sizeof(pos.x));
            inFile.read(reinterpret_cast<char*>(&pos.y), sizeof(pos.y));
            inFile.read(reinterpret_cast<char*>(&pos.z), sizeof(pos.z));
            // Читаем тип блока
            inFile.read(reinterpret_cast<char*>(&underlyingType), sizeof(underlyingType));

            if (inFile.fail()) {
                std::cerr << "ERROR::WORLDSTORAGE::LOADDELTA: Failed to read change record " << (i + 1) << " from file: " << filepath << std::endl;
                inFile.close();
                return DeltaStorage(); // Возвращаем пустую дельту при ошибке
            }

            // Преобразуем прочитанный тип обратно в BlockType
            type = static_cast<BlockType>(underlyingType);

            // Добавляем изменение в карту (используем setChange для возможной оптимизации)
            // Для загрузки можно напрямую вставлять в map для скорости:
            loadedDelta.changes[pos] = type; // Используем приватный доступ (или сделать loadChanges публичным)
            // Если DeltaStorage не friend, нужен публичный метод вроде addLoadedChange
        }

    }
    catch (const std::exception& e) {
        std::cerr << "ERROR::WORLDSTORAGE::LOADDELTA: Exception during loading: " << e.what() << std::endl;
        inFile.close();
        return DeltaStorage(); // Возвращаем пустую
    }

    inFile.close();

    // Проверяем, не осталось ли непрочитанных данных (на всякий случай)
    // inFile.peek(); // Попробовать прочитать еще байт
    // if (!inFile.eof()) {
    //     std::cerr << "Warning::WORLDSTORAGE::LOADDELTA: Extra data found at the end of file: " << filepath << std::endl;
    // }

    std::cout << "WorldStorage: Delta for world '" << worldName << "' loaded successfully (" << loadedDelta.getChangeCount() << " changes)." << std::endl;
    return loadedDelta; // Возвращаем загруженную дельту
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