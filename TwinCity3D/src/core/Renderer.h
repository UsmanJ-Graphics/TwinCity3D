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
        // Backface culling stays off for the MVP: GIS building footprints
        // arrive from OSM with inconsistent CW/CCW winding, and wall/roof
        // normals in CityMeshBuilder are computed geometrically rather than
        // from triangle order, so culling would incorrectly hide some faces.
        // Phase 19 (performance) can normalize winding during mesh build and
        // re-enable culling once the dataset is large enough to need it.
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
