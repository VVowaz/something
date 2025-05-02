#version 330 core
out vec4 FragColor;

in vec3 FragPos;   // Позиция фрагмента в мировых координатах
in vec3 Normal;
in vec2 TexCoords;

// uniform sampler2D texture1; // Для будущих текстур

// *** НОВЫЕ UNIFORMS ***
uniform ivec3 highlightedBlockPos; // Мировые координаты подсвечиваемого блока
uniform bool highlightActive;      // Активна ли подсветка для этого чанка/блока?

void main()
{
    // Базовый цвет (по нормали, как раньше)
    vec3 baseColor;
    if (Normal.y > 0.95) {
        baseColor = vec3(0.0, 0.6, 0.1); // Зеленый (трава)
    } else {
        baseColor = vec3(0.5, 0.5, 0.5); // Серый (камень/бока)
        // Добавим небольшое изменение цвета для боковых граней
        if (abs(Normal.y) < 0.95) {
             baseColor *= 0.9; // Слегка темнее
        }
    }

    // *** ЛОГИКА ПОДСВЕТКИ ***
    vec3 finalColor = baseColor;
    if (highlightActive) {
        // Вычисляем мировые координаты центра текущего блока, которому принадлежит фрагмент
        ivec3 currentBlockPos = ivec3(floor(FragPos.x), floor(FragPos.y), floor(FragPos.z));

        // Сравниваем с подсвечиваемым блоком
        if (currentBlockPos == highlightedBlockPos) {
            // Добавляем красный оттенок или делаем ярче
             finalColor = mix(baseColor, vec3(1.0, 0.2, 0.2), 0.4); // Смешиваем с красным
             // finalColor = baseColor * 1.3; // Или просто делаем ярче
        }
    }
    // *** КОНЕЦ ЛОГИКИ ПОДСВЕТКИ ***


    // Простое затенение (без изменений)
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, -0.3));
    float diff = max(dot(norm, lightDir), 0.25); // Увеличим ambient
    finalColor = finalColor * diff;

    FragColor = vec4(finalColor, 1.0f);
}