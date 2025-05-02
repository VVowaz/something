#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <array> // Для хранения плоскостей

// Defines possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum class CameraMovement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    UP,
    DOWN
};



// *** НОВОЕ: Структура для представления плоскости Ax + By + Cz + D = 0 ***
struct Plane {
    glm::vec3 normal = { 0.f, 1.f, 0.f }; // Нормаль плоскости (A, B, C)
    float distance = 0.f;                 // Смещение D от начала координат

    // Нормализация плоскости (чтобы нормаль имела длину 1)
    void normalize() {
        float mag = glm::length(normal);
        if (mag > 0.00001f) { // Избегаем деления на ноль
            normal /= mag;
            distance /= mag;
        }
    }
};

// Параметры камеры по умолчанию
const float YAW = -90.0f; /* ... */
const float PITCH = 0.0f;  /* ... */
const float SPEED = 5.0f;  /* ... */
const float SENSITIVITY = 0.1f;  /* ... */
const float FOV = 45.0f;  /* ... */
const float NEAR_PLANE = 0.1f;  /* ... */
const float FAR_PLANE = 200.0f; /* ... */

// An abstract camera class that processes input and calculates the corresponding Euler Angles, Vectors and Matrices for use in OpenGL
class Camera
{
public:
    // camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;
    // euler Angles
    float Yaw;
    float Pitch;
    // camera options
    float MovementSpeed;
    float MouseSensitivity;
    float Fov;
    float AspectRatio;
    float NearPlane;
    float FarPlane;

    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH);
    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch);

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 getViewMatrix() const;
    // returns the projection matrix calculated using perspective projection
    glm::mat4 getProjectionMatrix() const;
    // sets the aspect ratio
    void setAspectRatio(float ratio);

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void processKeyboard(CameraMovement direction, float deltaTime);
    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void processMouseScroll(float yoffset);

    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors();
    // *** НОВЫЕ МЕТОДЫ ДЛЯ FRUSTUM CULLING ***
    // Получить комбинированную матрицу View * Projection
    glm::mat4 getViewProjectionMatrix() const;
    // Получить массив из 6 плоскостей фрустума
    std::array<Plane, 6> getFrustumPlanes() const;

};