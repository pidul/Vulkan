#pragma once
#include "CommonHeaders.h"

enum class MoveDirection {
    up = 0,
    down,
    left,
    right
};

class Camera {
public:
    glm::vec3 m_Position;
    glm::vec3 m_LookAt;

    void MovePosition(MoveDirection dir);
    void MoveTarget(MoveDirection dir);
    glm::mat4 GetViewMatrix() {
        return glm::lookAt(m_Position, m_LookAt, glm::vec3(0.0f, 1.0f, 0.0f));
    }
};
