#include "Camera.h"

void Camera::MovePosition(MoveDirection dir) {
    glm::vec3 moveDirVector = m_Position - m_LookAt;
    moveDirVector.z = 0.0f;
    moveDirVector = glm::normalize(moveDirVector);
    if (dir == MoveDirection::up) {
        m_Position -= moveDirVector * 0.1f;
        m_LookAt -= moveDirVector * 0.1f;
        return;
    }
    else if (dir == MoveDirection::down) {
        m_Position += moveDirVector * 0.1f;
        m_LookAt += moveDirVector * 0.1f;
        return;
    }

    glm::vec3 perp(-moveDirVector.y, moveDirVector.x, 0.0f);
    perp = glm::normalize(perp);

    if (dir == MoveDirection::left) {
        m_Position -= perp * 0.1f;
        m_LookAt -= perp * 0.1f;
    }
    else if (dir == MoveDirection::right) {
        m_Position += perp * 0.1f;
        m_LookAt += perp * 0.1f;
    }
}

void Camera::MoveTarget(MoveDirection dir) {
    glm::vec3 moveDirVector = m_Position - m_LookAt;
    moveDirVector.z = 0.0f;
    moveDirVector = glm::normalize(moveDirVector);

    glm::vec3 perp(-moveDirVector.y, moveDirVector.x, 0.0f);
    if (dir == MoveDirection::up) {
        m_LookAt.z -= 0.1f;
        return;
    }
    else if (dir == MoveDirection::down) {
        m_LookAt.z += 0.1f;
        return;
    }

    perp = glm::vec3(-moveDirVector.y, moveDirVector.x, 0.0f);
    perp = glm::normalize(perp);

    if (dir == MoveDirection::left) {
        m_LookAt -= perp * 0.1f;
    }
    else if (dir == MoveDirection::right) {
        m_LookAt += perp * 0.1f;
    }
}
