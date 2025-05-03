#pragma once

#include "Block.h"      // Для BlockType
#include "Utils.h"      // Для ivec3_less (или VectorUtils.h)
#include <map>
#include <glm/glm.hpp>
#include <optional>     // Для std::optional (C++17)

// Класс для хранения изменений мира (дельта) в памяти.
// Использует map для быстрого поиска изменений по координатам.
class DeltaStorage {
public:
    // Тип карты: ключ - мировые координаты блока, значение - новый тип блока
    using DeltaMap = std::map<glm::ivec3, BlockType, ivec3_less>;

    DeltaStorage() = default; // Конструктор по умолчанию
    ~DeltaStorage() = default; // Деструктор по умолчанию

    // Запрещаем копирование (может быть большим)
    DeltaStorage(const DeltaStorage&) = delete;
    DeltaStorage& operator=(const DeltaStorage&) = delete;
    // Разрешаем перемещение
    DeltaStorage(DeltaStorage&&) = default;
    DeltaStorage& operator=(DeltaStorage&&) = default;

    // Добавить или обновить изменение для блока
    // baseBlockType - это тип блока, который был бы сгенерирован процедурно
    void setChange(const glm::ivec3& pos, BlockType newType, BlockType baseBlockType) {
        // Оптимизация: Если новый тип совпадает с базовым (сгенерированным),
        // то изменение можно удалить из дельты, так как блок вернулся
        // к своему "естественному" состоянию.
        if (newType == baseBlockType) {
            removeChange(pos); // Удаляем запись, если она есть
        }
        else {
            // Иначе, добавляем или обновляем запись в карте
            changes[pos] = newType;
        }
    }

    // Удалить изменение для блока (если игрок вернул блок к исходному состоянию)
    void removeChange(const glm::ivec3& pos) {
        changes.erase(pos); // erase безопасно вызывать, даже если ключа нет
    }

    // Получить измененный тип блока по координатам.
    // Возвращает std::optional<BlockType>. Если для координат нет изменений,
    // возвращает std::nullopt (пустой optional).
    std::optional<BlockType> getChange(const glm::ivec3& pos) const {
        auto it = changes.find(pos); // Ищем координаты в карте
        if (it != changes.end()) {
            // Нашли изменение - возвращаем тип из карты
            return it->second;
        }
        else {
            // Изменений для этих координат нет
            return std::nullopt;
        }
    }

    // Получить доступ ко всей карте изменений (например, для сохранения)
    const DeltaMap& getAllChanges() const {
        return changes;
    }

    // Очистить все изменения
    void clear() {
        changes.clear();
    }

    // Загрузить изменения (например, из WorldStorage)
    // Принимает map по значению или rvalue-ссылке для эффективности
    void loadChanges(DeltaMap loadedChanges) {
        changes = std::move(loadedChanges); // Перемещаем данные
    }

    // Получить количество изменений
    size_t getChangeCount() const {
        return changes.size();
    }

    DeltaMap changes;
};