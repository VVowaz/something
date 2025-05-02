#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

// Пока нет текстур, определяем цвет по нормали (Y-компоненте)
// Это даст зеленый цвет для верхних граней и серый для остальных

void main()
{
    vec3 color;
    // Проверяем Y-компоненту нормали (с небольшим допуском)
    if (Normal.y > 0.95) { // Верхняя грань (трава)
        color = vec3(0.0, 0.6, 0.1); // Зеленый
    } else if (Normal.y < -0.95) { // Нижняя грань (камень/земля)
         color = vec3(0.5, 0.5, 0.5); // Серый
    }
    else { // Боковые грани (камень/земля)
        color = vec3(0.55, 0.55, 0.55); // Чуть светлее серого
    }

    // Добавим простое затенение
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(vec3(0.5, 1.0, -0.3)); // Направление света
    float diff = max(dot(norm, lightDir), 0.2); // Рассеянный свет (минимум 0.2 для ambient)
    vec3 finalColor = color * diff;

    FragColor = vec4(finalColor, 1.0f);

    // Старый вариант (просто серый):
    // FragColor = vec4(0.5f, 0.5f, 0.5f, 1.0f);
}