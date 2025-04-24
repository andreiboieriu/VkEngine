#pragma once

struct Transform {
    glm::vec3 position{};
    glm::quat rotation = glm::quat(glm::vec3(0.f, 0.f, 0.f));
    glm::vec3 scale{1.f};

    glm::mat4 localMatrix{1.f};
    glm::mat4 globalMatrix{1.f};

    glm::vec3 forward;
    glm::vec3 right;

    Transform();
    Transform(const Transform&) = default;
    Transform(glm::vec3 position, glm::vec3 rotation, glm::vec3 scale);

    glm::mat4 getRotationMatrix();
    void update(glm::mat4 parentMatrix);
    void translate(glm::vec3 direction, float distance);
    void rotate(glm::vec3 axis, float degrees);
};
