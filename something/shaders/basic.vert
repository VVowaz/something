#version 330 core
layout (location = 0) in vec3 aPos;   // Позиция вершины
layout (location = 1) in vec3 aNormal; // Нормаль вершины
layout (location = 2) in vec2 aTexCoords; // Текстурные координаты

out vec3 FragPos;       // Позиция фрагмента в мировых координатах (для освещения)
out vec3 Normal;        // Нормаль (для освещения)
out vec2 TexCoords;     // Текстурные координаты

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0)); // Позиция в мировых координатах
    // Нормаль трансформируем с помощью нормальной матрицы (инверсно-транспонированной model)
    // Для простоты пока просто передаем локальную нормаль (работает для uniform масштаба)
    Normal = mat3(transpose(inverse(model))) * aNormal; // Правильный способ
    // Normal = aNormal; // Упрощенный способ (неправильно при non-uniform scale)

    TexCoords = aTexCoords; // Передаем текстурные координаты

    gl_Position = projection * view * model * vec4(aPos, 1.0); // Позиция в clip space
}