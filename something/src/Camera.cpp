#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

// Constructor with vectors
Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch) :
    Position(position), WorldUp(up), Yaw(yaw), Pitch(pitch),
    Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Fov(FOV),
    AspectRatio(16.0f / 9.0f), NearPlane(NEAR_PLANE), FarPlane(FAR_PLANE)
{
    updateCameraVectors();
}

// Constructor with scalar values
Camera::Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw, float pitch) :
    Position(glm::vec3(posX, posY, posZ)), WorldUp(glm::vec3(upX, upY, upZ)), Yaw(yaw), Pitch(pitch),
    Front(glm::vec3(0.0f, 0.0f, -1.0f)), MovementSpeed(SPEED), MouseSensitivity(SENSITIVITY), Fov(FOV),
    AspectRatio(16.0f / 9.0f), NearPlane(NEAR_PLANE), FarPlane(FAR_PLANE)
{
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(Position, Position + Front, Up);
}

glm::mat4 Camera::getProjectionMatrix() const
{
    float currentAspectRatio = (AspectRatio <= 0.0f) ? (16.0f / 9.0f) : AspectRatio;
    return glm::perspective(glm::radians(Fov), currentAspectRatio, NearPlane, FarPlane);
}

void Camera::setAspectRatio(float ratio) {
    if (ratio > 0.0f) {
        AspectRatio = ratio;
    }
    else {
        AspectRatio = 16.0f / 9.0f; // Safe default
        std::cerr << "Warning: Invalid aspect ratio provided to camera: " << ratio << ". Using default." << std::endl;
    }
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = MovementSpeed * deltaTime;
    if (direction == CameraMovement::FORWARD)
        Position += Front * velocity;
    if (direction == CameraMovement::BACKWARD)
        Position -= Front * velocity;
    if (direction == CameraMovement::LEFT)
        Position -= Right * velocity;
    if (direction == CameraMovement::RIGHT)
        Position += Right * velocity;
    if (direction == CameraMovement::UP)
        Position += WorldUp * velocity; // Use WorldUp for consistent up/down movement
    if (direction == CameraMovement::DOWN)
        Position -= WorldUp * velocity;
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    Yaw += xoffset;
    Pitch += yoffset;

    // make sure that when pitch is out of bounds, screen doesn't get flipped
    if (constrainPitch)
    {
        if (Pitch > 89.0f)
            Pitch = 89.0f;
        if (Pitch < -89.0f)
            Pitch = -89.0f;
    }

    // update Front, Right and Up Vectors using the updated Euler angles
    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset)
{
    // Basic FOV zoom
    Fov -= (float)yoffset;
    if (Fov < 1.0f)
        Fov = 1.0f;
    if (Fov > 60.0f) // Adjust max FOV if needed
        Fov = 60.0f;
    // std::cout << "Camera FOV: " << Fov << std::endl;
}

void Camera::updateCameraVectors()
{
    // calculate the new Front vector
    glm::vec3 frontCalculated;
    frontCalculated.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    frontCalculated.y = sin(glm::radians(Pitch));
    frontCalculated.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    Front = glm::normalize(frontCalculated);
    // also re-calculate the Right and Up vector
    Right = glm::normalize(glm::cross(Front, WorldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
    Up = glm::normalize(glm::cross(Right, Front));
}

glm::mat4 Camera::getViewProjectionMatrix() const {
    // Важно: Сначала Projection, потом View (умножение справа налево)
    return getProjectionMatrix() * getViewMatrix();
}

std::array<Plane, 6> Camera::getFrustumPlanes() const {
    std::array<Plane, 6> planes;
    glm::mat4 vp = getViewProjectionMatrix(); // Получаем View-Projection матрицу

    // Извлекаем плоскости
    // Формулы основаны на стандартных методах извлечения плоскостей из матрицы VP
    // Источник: http://www.cs.otago.ac.nz/postgrads/alexis/planeExtraction.pdf (и другие)

    // Левая плоскость (Left)
    planes[0].normal.x = vp[0][3] + vp[0][0];
    planes[0].normal.y = vp[1][3] + vp[1][0];
    planes[0].normal.z = vp[2][3] + vp[2][0];
    planes[0].distance = vp[3][3] + vp[3][0];

    // Правая плоскость (Right)
    planes[1].normal.x = vp[0][3] - vp[0][0];
    planes[1].normal.y = vp[1][3] - vp[1][0];
    planes[1].normal.z = vp[2][3] - vp[2][0];
    planes[1].distance = vp[3][3] - vp[3][0];

    // Нижняя плоскость (Bottom)
    planes[2].normal.x = vp[0][3] + vp[0][1];
    planes[2].normal.y = vp[1][3] + vp[1][1];
    planes[2].normal.z = vp[2][3] + vp[2][1];
    planes[2].distance = vp[3][3] + vp[3][1];

    // Верхняя плоскость (Top)
    planes[3].normal.x = vp[0][3] - vp[0][1];
    planes[3].normal.y = vp[1][3] - vp[1][1];
    planes[3].normal.z = vp[2][3] - vp[2][1];
    planes[3].distance = vp[3][3] - vp[3][1];

    // Ближняя плоскость (Near)
    planes[4].normal.x = vp[0][3] + vp[0][2];
    planes[4].normal.y = vp[1][3] + vp[1][2];
    planes[4].normal.z = vp[2][3] + vp[2][2];
    planes[4].distance = vp[3][3] + vp[3][2];

    // Дальняя плоскость (Far)
    planes[5].normal.x = vp[0][3] - vp[0][2];
    planes[5].normal.y = vp[1][3] - vp[1][2];
    planes[5].normal.z = vp[2][3] - vp[2][2];
    planes[5].distance = vp[3][3] - vp[3][2];

    // Нормализуем все плоскости
    for (int i = 0; i < 6; ++i) {
        planes[i].normalize();
    }

    return planes;
}