#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace twin {

// Free-fly FPS camera. Positions are expressed in the digital twin's local
// Cartesian coordinate system (meters), not lat/lon. See gis/CoordinateSystem
// for the lat/lon -> local conversion used by the GIS pipeline.
class Camera {
public:
    Camera(glm::vec3 startPosition = glm::vec3(0.0f, 40.0f, 80.0f),
           float startYaw = -90.0f,
           float startPitch = -20.0f)
        : m_defaultPosition(startPosition),
          m_defaultYaw(startYaw),
          m_defaultPitch(startPitch) {
        Reset();
    }

    void Reset() {
        m_position = m_defaultPosition;
        m_yaw = m_defaultYaw;
        m_pitch = m_defaultPitch;
        m_fovDegrees = 60.0f;
        UpdateVectors();
    }

    // dt in seconds, direction flags describe which movement keys are held.
    void ProcessKeyboard(bool forward, bool backward, bool left, bool right,
                          bool up, bool down, float dt) {
        float velocity = m_moveSpeed * dt;
        if (forward)  m_position += m_front * velocity;
        if (backward) m_position -= m_front * velocity;
        if (left)     m_position -= m_right * velocity;
        if (right)    m_position += m_right * velocity;
        if (up)       m_position += m_worldUp * velocity;
        if (down)     m_position -= m_worldUp * velocity;
    }

    void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true) {
        xOffset *= m_mouseSensitivity;
        yOffset *= m_mouseSensitivity;

        m_yaw += xOffset;
        m_pitch += yOffset;

        if (constrainPitch) {
            if (m_pitch > 89.0f) m_pitch = 89.0f;
            if (m_pitch < -89.0f) m_pitch = -89.0f;
        }
        UpdateVectors();
    }

    void ProcessScroll(float yOffset) {
        m_fovDegrees -= yOffset;
        if (m_fovDegrees < 10.0f) m_fovDegrees = 10.0f;
        if (m_fovDegrees > 90.0f) m_fovDegrees = 90.0f;
    }

    glm::mat4 GetViewMatrix() const {
        return glm::lookAt(m_position, m_position + m_front, m_up);
    }

    glm::mat4 GetProjectionMatrix(float aspectRatio) const {
        return glm::perspective(glm::radians(m_fovDegrees), aspectRatio, 0.1f, 5000.0f);
    }

    const glm::vec3& GetPosition() const { return m_position; }
    float GetFov() const { return m_fovDegrees; }

private:
    void UpdateVectors() {
        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        m_front = glm::normalize(front);
        m_right = glm::normalize(glm::cross(m_front, m_worldUp));
        m_up = glm::normalize(glm::cross(m_right, m_front));
    }

    glm::vec3 m_position{};
    glm::vec3 m_front{};
    glm::vec3 m_up{};
    glm::vec3 m_right{};
    glm::vec3 m_worldUp{0.0f, 1.0f, 0.0f};

    float m_yaw{};
    float m_pitch{};
    float m_fovDegrees{60.0f};

    float m_moveSpeed{25.0f};
    float m_mouseSensitivity{0.1f};

    glm::vec3 m_defaultPosition;
    float m_defaultYaw;
    float m_defaultPitch;
};

}  // namespace twin
