#pragma once

struct Transform;

struct Camera {
    float fov = 70.f;
    float aspectRatio;
    float near = 0.1f;
    float far = 1000.f;

    bool enabled = false;

    Camera();
    Camera(const Camera&) = default;

    void updateMatrices(const Transform& transform);

    glm::mat4 viewMatrix = glm::mat4(1.f);
    glm::mat4 projectionMatrix = glm::mat4(1.f);
    glm::vec3 globalPosition = glm::vec3(0.f);
};
