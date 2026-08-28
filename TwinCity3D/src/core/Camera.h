#pragma once
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace twin {

    // Phase 12: which control scheme currently drives the camera. FreeFly is
    // the original Phase 0 FPS camera (WASD + mouse-look), unchanged in
    // behavior. Orbit/TopDown/Isometric are new: all three look at a shared
    // m_orbitTarget point and are driven by drag-to-rotate/pan + scroll-to-zoom
    // instead of WASD — matching how GIS/CAD viewers conventionally work,
    // rather than reusing FPS controls for what's meant to be a top-down
    // planning view.
    enum class CameraMode {
        FreeFly,
        Orbit,
        TopDown,
        Isometric
    };

    // Free-fly FPS camera (Phase 0), extended in Phase 12 with Orbit/TopDown/
    // Isometric modes and smooth (interpolated) transitions between camera
    // states — mode switches, and FocusOn() (Phase 11's zone-focus, previously
    // a hard cut) all now animate into position over kTransitionDuration
    // seconds instead of snapping, per the master spec's Phase 12 "smooth
    // transitions between camera states" requirement. Reset() remains an
    // immediate cut by design (see its doc comment).
    //
    // Positions are expressed in the digital twin's local Cartesian coordinate
    // system (meters), not lat/lon. See gis/CoordinateSystem for the lat/lon ->
    // local conversion used by the GIS pipeline.
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

        // Snaps back to the Phase 0 default free-fly pose and cancels any
        // in-flight transition. Unlike FocusOn()/SetMode(), this is an
        // immediate cut, not a smooth transition — "R" is meant to feel like an
        // emergency "get me back to a known state" key, not a scenic move.
        void Reset() {
            m_mode = CameraMode::FreeFly;
            m_isTransitioning = false;
            m_position = m_defaultPosition;
            m_yaw = m_defaultYaw;
            m_pitch = m_defaultPitch;
            m_fovDegrees = 60.0f;
            m_orbitTarget = glm::vec3(0.0f);
            m_orbitDistance = glm::length(m_position - m_orbitTarget);
            m_orbitYaw = m_yaw;
            m_orbitPitch = m_pitch;
            UpdateVectors();
        }

        CameraMode GetMode() const { return m_mode; }

        // Phase 12: switches control scheme, always animating smoothly into the
        // new mode's pose (fixed angles for TopDown/Isometric; whatever orbit
        // angle we already had for Orbit) rather than cutting instantly, so a
        // mode switch reads as "the camera flies to a new vantage point," not a
        // jarring pop.
        void SetMode(CameraMode mode) {
            if (mode == m_mode && !m_isTransitioning) return;

            // Coming from FreeFly, there's no existing orbit target — guess one
            // from where the camera is currently looking (ground-plane hit),
            // so Orbit/TopDown/Isometric orbit around something sensible rather
            // than an arbitrary leftover point. If we're already in an orbit-
            // family mode, keep whatever target we have (e.g. a Phase 11
            // FocusOn() zone) instead of re-deriving and overwriting it.
            if (m_mode == CameraMode::FreeFly && mode != CameraMode::FreeFly) {
                glm::vec3 groundHit;
                if (IntersectOwnForward(groundHit)) {
                    m_orbitTarget = groundHit;
                }
                m_orbitDistance = glm::clamp(glm::length(m_position - m_orbitTarget),
                    kMinOrbitDistance, kMaxOrbitDistance);
                m_orbitYaw = m_yaw;
                m_orbitPitch = m_pitch;
            }

            m_mode = mode;

            float targetYaw = m_yaw;
            float targetPitch = m_pitch;
            switch (mode) {
            case CameraMode::FreeFly:
            case CameraMode::Orbit:
                targetYaw = m_orbitYaw;
                targetPitch = m_orbitPitch;
                break;
            case CameraMode::TopDown:
                targetYaw = kTopDownYaw;
                targetPitch = kTopDownPitch;
                break;
            case CameraMode::Isometric:
                targetYaw = kIsometricYaw;
                targetPitch = kIsometricPitch;
                break;
            }

            glm::vec3 targetPosition = m_orbitTarget - ForwardFromAngles(targetYaw, targetPitch) * m_orbitDistance;
            StartTransition(targetPosition, targetYaw, targetPitch, m_fovDegrees);
        }

        // Phase 11 (hard cut) / Phase 12 (now smooth): re-targets the camera at
        // `targetPosition` — typically a zone's ground-level center — and
        // switches into Orbit mode if currently in FreeFly (so "focus this
        // zone" always ends in a controllable orbit around it). If already in
        // Orbit/TopDown/Isometric, the current mode is kept and only the
        // target/distance move — focusing a new zone while already in TopDown
        // stays top-down.
        void FocusOn(const glm::vec3& targetPosition,
            float viewDistance = 140.0f,
            float heightAboveTarget = 90.0f) {
            m_orbitTarget = targetPosition;
            m_orbitDistance = glm::clamp(std::sqrt(viewDistance * viewDistance + heightAboveTarget * heightAboveTarget),
                kMinOrbitDistance, kMaxOrbitDistance);
            m_orbitPitch = -glm::degrees(std::atan2(heightAboveTarget, viewDistance));
            m_orbitYaw = m_yaw;  // keep whatever heading we already had; only pitch/distance are prescribed

            m_mode = (m_mode == CameraMode::FreeFly) ? CameraMode::Orbit : m_mode;

            float targetYaw, targetPitch;
            switch (m_mode) {
            case CameraMode::TopDown:   targetYaw = kTopDownYaw;   targetPitch = kTopDownPitch;   break;
            case CameraMode::Isometric: targetYaw = kIsometricYaw; targetPitch = kIsometricPitch; break;
            default:                    targetYaw = m_orbitYaw;    targetPitch = m_orbitPitch;    break;
            }

            glm::vec3 targetPos = m_orbitTarget - ForwardFromAngles(targetYaw, targetPitch) * m_orbitDistance;
            StartTransition(targetPos, targetYaw, targetPitch, m_fovDegrees);
        }

        // Phase 12: advances the current smooth transition (if any) and,
        // once idle, keeps m_position synced to the orbit parameters for
        // Orbit/TopDown/Isometric — those three are always "some distance/angle
        // from m_orbitTarget," never freely flown, so their pose is a pure
        // function of orbit state rather than something ProcessKeyboard/
        // ProcessMouseMovement write directly. Must be called once per frame;
        // dt in seconds.
        void Tick(float dt) {
            if (m_isTransitioning) {
                m_transitionElapsed += dt;
                float t = glm::clamp(m_transitionElapsed / kTransitionDuration, 0.0f, 1.0f);
                float eased = t * t * (3.0f - 2.0f * t);  // smoothstep

                m_position = glm::mix(m_transitionStartPos, m_transitionTargetPos, eased);
                m_yaw = LerpAngle(m_transitionStartYaw, m_transitionTargetYaw, eased);
                m_pitch = glm::mix(m_transitionStartPitch, m_transitionTargetPitch, eased);
                m_fovDegrees = glm::mix(m_transitionStartFov, m_transitionTargetFov, eased);
                UpdateVectors();

                if (t >= 1.0f) {
                    m_isTransitioning = false;
                    if (m_mode == CameraMode::Orbit) {
                        m_orbitYaw = m_yaw;
                        m_orbitPitch = m_pitch;
                    }
                }
                return;
            }

            if (m_mode != CameraMode::FreeFly) {
                RecomputeOrbitPose();
            }
        }

        // --- FreeFly controls (unchanged behavior; no-ops outside FreeFly) ---

        void ProcessKeyboard(bool forward, bool backward, bool left, bool right,
            bool up, bool down, float dt) {
            if (m_mode != CameraMode::FreeFly) return;
            if (forward || backward || left || right || up || down) CancelTransition();

            float velocity = m_moveSpeed * dt;
            if (forward)  m_position += m_front * velocity;
            if (backward) m_position -= m_front * velocity;
            if (left)     m_position -= m_right * velocity;
            if (right)    m_position += m_right * velocity;
            if (up)       m_position += m_worldUp * velocity;
            if (down)     m_position -= m_worldUp * velocity;
        }

        void ProcessMouseMovement(float xOffset, float yOffset, bool constrainPitch = true) {
            if (m_mode != CameraMode::FreeFly) return;
            if (xOffset != 0.0f || yOffset != 0.0f) CancelTransition();

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

        // --- Orbit/TopDown/Isometric controls (no-ops in FreeFly) ---

        // Rotates around m_orbitTarget. Only has an effect in Orbit mode —
        // TopDown/Isometric intentionally hold a fixed viewing angle (locked
        // top-down / classic isometric, per the master spec), so a drag there
        // is routed to PanDrag() instead by Application's input dispatch.
        void OrbitDrag(float dxPixels, float dyPixels) {
            if (m_mode != CameraMode::Orbit) return;
            CancelTransition();
            m_orbitYaw += dxPixels * m_orbitSensitivity;
            m_orbitPitch -= dyPixels * m_orbitSensitivity;
            m_orbitPitch = glm::clamp(m_orbitPitch, -89.0f, -5.0f);
        }

        // Pans m_orbitTarget along the current view's screen-right/-forward
        // (flattened onto the ground plane), so dragging feels like sliding a
        // map rather than strafing in 3D. Used for right-drag in Orbit, and
        // left-drag in TopDown/Isometric (which don't rotate).
        void PanDrag(float dxPixels, float dyPixels) {
            if (m_mode == CameraMode::FreeFly) return;
            CancelTransition();
            float panScale = m_orbitDistance * m_panSensitivity;
            glm::vec3 forwardFlat = glm::vec3(m_front.x, 0.0f, m_front.z);
            glm::vec3 rightFlat = glm::vec3(m_right.x, 0.0f, m_right.z);
            if (glm::length(forwardFlat) > 1e-5f) forwardFlat = glm::normalize(forwardFlat);
            if (glm::length(rightFlat) > 1e-5f) rightFlat = glm::normalize(rightFlat);
            m_orbitTarget += (-rightFlat * dxPixels + forwardFlat * dyPixels) * panScale;
        }

        // FOV zoom in FreeFly (unchanged Phase 0 behavior); orbit-distance zoom
        // (dolly) in every other mode.
        void ProcessScroll(float yOffset) {
            if (m_mode == CameraMode::FreeFly) {
                m_fovDegrees -= yOffset;
                if (m_fovDegrees < 10.0f) m_fovDegrees = 10.0f;
                if (m_fovDegrees > 90.0f) m_fovDegrees = 90.0f;
                return;
            }
            CancelTransition();
            m_orbitDistance -= yOffset * (m_orbitDistance * 0.1f);
            m_orbitDistance = glm::clamp(m_orbitDistance, kMinOrbitDistance, kMaxOrbitDistance);
        }

        glm::mat4 GetViewMatrix() const {
            return glm::lookAt(m_position, m_position + m_front, m_up);
        }

        glm::mat4 GetProjectionMatrix(float aspectRatio) const {
            return glm::perspective(glm::radians(m_fovDegrees), aspectRatio, 0.1f, 5000.0f);
        }

        const glm::vec3& GetPosition() const { return m_position; }
        float GetFov() const { return m_fovDegrees; }
        bool IsTransitioning() const { return m_isTransitioning; }

    private:
        static constexpr float kTransitionDuration = 0.6f;
        static constexpr float kMinOrbitDistance = 15.0f;
        static constexpr float kMaxOrbitDistance = 2000.0f;
        static constexpr float kTopDownYaw = -90.0f;
        static constexpr float kTopDownPitch = -89.9f;
        static constexpr float kIsometricYaw = -135.0f;
        static constexpr float kIsometricPitch = -35.264f;  // classic isometric angle

        static glm::vec3 ForwardFromAngles(float yawDeg, float pitchDeg) {
            glm::vec3 f;
            f.x = cos(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
            f.y = sin(glm::radians(pitchDeg));
            f.z = sin(glm::radians(yawDeg)) * cos(glm::radians(pitchDeg));
            return glm::normalize(f);
        }

        // Shortest-path angle lerp, so a transition never spins the long way
        // around (e.g. -170 -> 170 goes through 180, not back through 0).
        static float LerpAngle(float a, float b, float t) {
            float diff = std::fmod(b - a + 540.0f, 360.0f) - 180.0f;
            return a + diff * t;
        }

        void StartTransition(const glm::vec3& targetPos, float targetYaw, float targetPitch, float targetFov) {
            m_transitionStartPos = m_position;
            m_transitionStartYaw = m_yaw;
            m_transitionStartPitch = m_pitch;
            m_transitionStartFov = m_fovDegrees;
            m_transitionTargetPos = targetPos;
            m_transitionTargetYaw = targetYaw;
            m_transitionTargetPitch = targetPitch;
            m_transitionTargetFov = targetFov;
            m_transitionElapsed = 0.0f;
            m_isTransitioning = true;
        }

        // Manual input (WASD, mouse-look, orbit/pan-drag, scroll) always wins
        // over an in-flight transition — the user touching the controls means
        // "give me control now," not "wait for the animation to finish."
        void CancelTransition() {
            if (!m_isTransitioning) return;
            m_isTransitioning = false;
            if (m_mode == CameraMode::Orbit) {
                m_orbitYaw = m_yaw;
                m_orbitPitch = m_pitch;
            }
        }

        // Re-derives m_position (and m_front/m_right/m_up) from m_orbitTarget +
        // current yaw/pitch/distance for Orbit/TopDown/Isometric. In Orbit mode
        // m_yaw/m_pitch track m_orbitYaw/m_orbitPitch (which OrbitDrag() edits);
        // in TopDown/Isometric they stay at their fixed values (set once by
        // SetMode()/FocusOn()'s transition target and never touched again,
        // since OrbitDrag() only acts in Orbit mode) — either way this simply
        // re-derives position from whatever yaw/pitch/distance/target currently
        // are.
        void RecomputeOrbitPose() {
            if (m_mode == CameraMode::Orbit) {
                m_yaw = m_orbitYaw;
                m_pitch = m_orbitPitch;
            }
            m_position = m_orbitTarget - ForwardFromAngles(m_yaw, m_pitch) * m_orbitDistance;
            UpdateVectors();
        }

        // Used by SetMode() when leaving FreeFly: intersects the camera's
        // current forward ray with the ground plane (y=0) to guess a sensible
        // orbit target, so switching into Orbit from a free-flown position
        // doesn't orbit around an arbitrary/undefined point. Returns false
        // (caller keeps the existing m_orbitTarget) if the camera is looking
        // near-parallel to the ground or upward — no meaningful ground hit.
        bool IntersectOwnForward(glm::vec3& outHit) const {
            if (std::fabs(m_front.y) < 1e-4f || m_front.y > 0.0f) return false;
            float t = -m_position.y / m_front.y;
            if (t <= 0.0f) return false;
            outHit = m_position + m_front * t;
            return true;
        }

        void UpdateVectors() {
            m_front = ForwardFromAngles(m_yaw, m_pitch);
            m_right = glm::normalize(glm::cross(m_front, m_worldUp));
            m_up = glm::normalize(glm::cross(m_right, m_front));
        }

        CameraMode m_mode{ CameraMode::FreeFly };

        glm::vec3 m_position{};
        glm::vec3 m_front{};
        glm::vec3 m_up{};
        glm::vec3 m_right{};
        glm::vec3 m_worldUp{ 0.0f, 1.0f, 0.0f };

        float m_yaw{};
        float m_pitch{};
        float m_fovDegrees{ 60.0f };

        float m_moveSpeed{ 25.0f };
        float m_mouseSensitivity{ 0.1f };
        float m_orbitSensitivity{ 0.25f };
        float m_panSensitivity{ 0.0015f };

        // Orbit/TopDown/Isometric shared state.
        glm::vec3 m_orbitTarget{ 0.0f };
        float m_orbitDistance{ 120.0f };
        float m_orbitYaw{ -90.0f };
        float m_orbitPitch{ -30.0f };

        // Smooth-transition state (Phase 12).
        bool m_isTransitioning{ false };
        float m_transitionElapsed{ 0.0f };
        glm::vec3 m_transitionStartPos{}, m_transitionTargetPos{};
        float m_transitionStartYaw{}, m_transitionTargetYaw{};
        float m_transitionStartPitch{}, m_transitionTargetPitch{};
        float m_transitionStartFov{}, m_transitionTargetFov{};

        glm::vec3 m_defaultPosition;
        float m_defaultYaw;
        float m_defaultPitch;
    };

}  // namespace twin