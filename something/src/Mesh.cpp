#include "Mesh.h"   // Включаем заголовок класса Mesh
#include <utility>  // Для std::move
#include <vector>   // Для std::vector (хотя он используется только в .h для объявления)
#include <iostream> // Для вывода ошибок (std::cerr)
#include <cstddef>  // Для offsetof

// --- Конструктор ---
// Принимает векторы нового формата Vertex и индексы GLuint
Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices) {
    // Вывод для отладки - можно закомментировать в релизе
    // std::cout << "Mesh: Initializing with " << vertices.size() << " vertices, " << indices.size() << " indices." << std::endl;

    // Проверяем, не пусты ли переданные данные
    if (vertices.empty() || indices.empty()) {
        std::cerr << "ERROR::MESH::CONSTRUCTOR: Received empty vertex or index data! Mesh will be invalid." << std::endl;
        // Оставляем VAO/VBO/EBO нулевыми, isValid() вернет false
    }
    else {
        // Если данные есть, вызываем настройку буферов
        setupMesh(vertices, indices);
        // Проверяем результат настройки
        if (!isValid()) {
            std::cerr << "ERROR::MESH::CONSTRUCTOR: Failed to set up mesh buffers correctly after setup call." << std::endl;
        } // else {
             // Сообщение об успехе теперь выводится в конце setupMesh, если нет ошибок
             // std::cout << "Mesh: Setup complete." << std::endl;
        //}
    }
}

// --- Деструктор ---
// Вызывает cleanup для освобождения ресурсов OpenGL
Mesh::~Mesh() {
    cleanup();
}

// --- Конструктор перемещения ---
// Перехватывает владение ресурсами OpenGL от другого объекта Mesh
Mesh::Mesh(Mesh&& other) noexcept
    : VAO(other.VAO), VBO(other.VBO), EBO(other.EBO), indexCount(other.indexCount)
{
    // Обнуляем ресурсы у "перемещенного" объекта, чтобы его деструктор их не удалил
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.indexCount = 0;
}

// --- Оператор присваивания перемещением ---
// Освобождает текущие ресурсы и перехватывает владение от другого объекта Mesh
Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) { // Защита от самоприсваивания
        cleanup(); // Очищаем текущие ресурсы

        // Перемещаем ресурсы от "other" к "this"
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        indexCount = other.indexCount;

        // Обнуляем ресурсы у "other"
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
        other.indexCount = 0;
    }
    return *this;
}

// --- Настройка меша ---
// Создает и настраивает VAO, VBO, EBO и атрибуты вершин
void Mesh::setupMesh(const std::vector<Vertex>& vertices, const std::vector<GLuint>& indices) {
    // Очистка предыдущих ошибок OpenGL (на всякий случай)
    while (glGetError() != GL_NO_ERROR);

    // Сохраняем количество индексов для отрисовки
    indexCount = static_cast<GLsizei>(indices.size());

    GLenum err; // Переменная для хранения кода ошибки OpenGL

    // 1. Генерируем объекты OpenGL (VAO, VBO, EBO)
    glGenVertexArrays(1, &VAO);
    err = glGetError(); if (VAO == 0 || err != GL_NO_ERROR) { std::cerr << "ERROR::MESH::SETUP: Failed to generate VAO. OpenGL Error: " << err << std::endl; cleanup(); return; }

    glGenBuffers(1, &VBO);
    err = glGetError(); if (VBO == 0 || err != GL_NO_ERROR) { std::cerr << "ERROR::MESH::SETUP: Failed to generate VBO. OpenGL Error: " << err << std::endl; cleanup(); return; }

    glGenBuffers(1, &EBO);
    err = glGetError(); if (EBO == 0 || err != GL_NO_ERROR) { std::cerr << "ERROR::MESH::SETUP: Failed to generate EBO. OpenGL Error: " << err << std::endl; cleanup(); return; }

    // 2. Привязываем VAO (все последующие настройки VBO, EBO, атрибутов будут сохранены в этом VAO)
    glBindVertexArray(VAO);
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error binding VAO: " << err << std::endl; cleanup(); return; }

    // 3. Настраиваем VBO (Vertex Buffer Object)
    glBindBuffer(GL_ARRAY_BUFFER, VBO); // Привязываем VBO к цели GL_ARRAY_BUFFER
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error binding VBO: " << err << std::endl; cleanup(); return; }
    // Копируем данные вершин из вектора в VBO
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error VBO BufferData: " << err << std::endl; cleanup(); return; }

    // 4. Настраиваем EBO (Element Buffer Object / Index Buffer)
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO); // Привязываем EBO к цели GL_ELEMENT_ARRAY_BUFFER
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error binding EBO: " << err << std::endl; cleanup(); return; }
    // Копируем данные индексов из вектора в EBO
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error EBO BufferData: " << err << std::endl; cleanup(); return; }

    // 5. Настраиваем Атрибуты Вершин (как читать данные из VBO)
    // Атрибут 0: Позиция (vec3)
    glEnableVertexAttribArray(0); // Включаем атрибут 0
    // Указываем OpenGL, как интерпретировать данные для атрибута 0:
    // - Индекс атрибута: 0 (location = 0 в шейдере)
    // - Количество компонентов: 3 (vec3)
    // - Тип компонентов: GL_FLOAT
    // - Нормализация: GL_FALSE (не нужно нормализовать)
    // - Шаг (Stride): sizeof(Vertex) - расстояние в байтах между началами данных для соседних вершин
    // - Смещение (Offset): (void*)offsetof(Vertex, Position) - смещение в байтах от начала структуры Vertex до поля Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error VertexAttribPointer (Pos): " << err << std::endl; cleanup(); return; }

    // Атрибут 1: Нормаль (vec3)
    glEnableVertexAttribArray(1); // Включаем атрибут 1 (location = 1 в шейдере)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error VertexAttribPointer (Normal): " << err << std::endl; cleanup(); return; }

    // Атрибут 2: Текстурные Координаты (vec2)
    glEnableVertexAttribArray(2); // Включаем атрибут 2 (location = 2 в шейдере)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
    err = glGetError(); if (err != GL_NO_ERROR) { std::cerr << "OpenGL error VertexAttribPointer (TexCoords): " << err << std::endl; cleanup(); return; }

    // 6. Отвязываем VAO, чтобы случайно не изменить его настройки.
    // VBO (привязанный к GL_ARRAY_BUFFER) и EBO (привязанный к GL_ELEMENT_ARRAY_BUFFER внутри VAO)
    // остаются связанными с этим VAO.
    glBindVertexArray(0);

    // Можно также отвязать GL_ARRAY_BUFFER, так как он больше не нужен для настройки VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // Отвязывать GL_ELEMENT_ARRAY_BUFFER *не нужно*, так как его привязка является частью состояния VAO.

    // Финальная проверка ошибок после всей настройки
    err = glGetError();
    if (err == GL_NO_ERROR) {
        // std::cout << "Mesh: Setup successful (VAO: " << VAO << ")" << std::endl;
    }
    else {
        std::cerr << "OpenGL error occurred during Mesh setup: " << err << std::endl;
        cleanup(); // Очищаем ресурсы, если была ошибка
    }
}

// --- Отрисовка меша ---
// Привязывает VAO и вызывает glDrawElements
void Mesh::draw() const {
    // Проверяем, был ли меш успешно создан
    if (!isValid()) {
        // std::cerr << "Warning::MESH::DRAW: Attempting to draw an invalid mesh (VAO=" << VAO << ")" << std::endl;
        return;
    }

    // Привязываем VAO. Это автоматически привязывает соответствующий VBO и EBO
    // и восстанавливает настройки атрибутов, сохраненные при создании VAO.
    glBindVertexArray(VAO);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error binding VAO for draw (VAO ID: " << VAO << "): " << err << std::endl;
        glBindVertexArray(0); // Попытка отвязать на всякий случай
        return;
    }

    // Выполняем отрисовку индексированных треугольников
    // - Режим: GL_TRIANGLES
    // - Количество индексов: indexCount (сохранено при настройке)
    // - Тип индексов: GL_UNSIGNED_INT (соответствует std::vector<GLuint>)
    // - Смещение в буфере индексов (EBO): 0 (начинаем с начала)
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "OpenGL error during glDrawElements (VAO ID: " << VAO << ", Index Count: " << indexCount << "): " << err << std::endl;
    }

    // Отвязываем VAO после отрисовки (хорошая практика, предотвращает случайные изменения)
    glBindVertexArray(0);
}


// --- Очистка ресурсов ---
// Освобождает ресурсы OpenGL (VAO, VBO, EBO)
void Mesh::cleanup() {
    // Удаляем буферы OpenGL, если они были созданы (ID > 0)
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0; // Сбрасываем ID
    }
    if (EBO != 0) {
        glDeleteBuffers(1, &EBO);
        EBO = 0; // Сбрасываем ID
    }
    // VAO удаляем в последнюю очередь
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0; // Сбрасываем ID
    }
    indexCount = 0; // Сбрасываем счетчик индексов
}