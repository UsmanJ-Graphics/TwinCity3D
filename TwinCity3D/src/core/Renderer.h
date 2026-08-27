#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>

namespace twin {

// Thin wrapper around global GL state so Application doesn't call raw GL
// directly. This is intentionally minimal for Phase 0: clear + depth test.
// Frustum culling / batching (Phase 19 performance work) hooks in here later
// without changing the Application's call site.
class Renderer {
public:
    void Init() {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, 1.0f);
    }

    void SetClearColor(const glm::vec3& color) { m_clearColor = color; }

    void BeginFrame(int framebufferWidth, int framebufferHeight) {
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(m_clearColor.r, m_clearColor.g, m_clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

private:
    glm::vec3 m_clearColor{0.53f, 0.72f, 0.88f};  // sky blue, placeholder
};

}  // namespace twin
