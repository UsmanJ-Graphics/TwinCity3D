#pragma once
#include <cmath>
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

    // Phase 11: repositions the camera to look at `targetPosition` (a
    // ground-level point, typically a zone's center) from a fixed angled
    // offset. Used by the Top Priority Zones panel so clicking an entry
    // visibly shows "you're now looking at this zone", not just an
    // Inspector panel update.
    //
    // Deliberately a HARD CUT, not an interpolated flight — smooth
    // camera transitions between states are explicitly Phase 12 scope
    // (master spec Phase 12: "Add smooth transitions between camera
    // states"). This is the minimum viable "focus camera on the zone"
    // Phase 11 needs; Phase 12 can animate m_position/m_yaw/m_pitch toward
    // the values this computes instead of snapping them, without touching
    // this method's contract.
    void FocusOn(const glm::vec3& targetPosition,
                 float viewDistance = 140.0f,
                 float heightAboveTarget = 90.0f) {
        glm::vec3 offset(0.0f, heightAboveTarget, viewDistance);
        m_position = targetPosition + offset;

        glm::vec3 dir = glm::normalize(targetPosition - m_position);
        m_pitch = glm::degrees(asin(dir.y));
        m_yaw = glm::degrees(atan2(dir.z, dir.x));
        UpdateVectors();
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
