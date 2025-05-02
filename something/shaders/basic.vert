#version 330 core
layout (location = 0) in vec3 aPos;   // Локальная позиция вершины в меше чанка
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec3 FragPos;       // Позиция фрагмента в МИРОВЫХ координатах
out vec3 Normal;
out vec2 TexCoords;

uniform mat4 model;      // Матрица модели (перенос чанка в мир)
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Трансформируем локальную позицию вершины в мировые координаты
    FragPos = vec3(model * vec4(aPos, 1.0));
    // Трансформируем нормаль (правильно)
    Normal = mat3(transpose(inverse(model))) * aNormal;
    // Передаем текстурные координаты
    TexCoords = aTexCoords;

    // Вычисляем позицию в пространстве отсечения
    gl_Position = projection * view * vec4(FragPos, 1.0); // Используем уже посчитанный FragPos
}