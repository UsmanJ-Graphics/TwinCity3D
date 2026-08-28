#pragma once
#include <glm/glm.hpp>
#include <vector>

#include "Camera.h"
#include "../twin/Zone.h"

namespace twin {

    // A world-space ray: origin + normalized direction.
    struct Ray {
        glm::vec3 origin{ 0.0f };
        glm::vec3 direction{ 0.0f, 0.0f, -1.0f };
    };

    // Phase 10: turns a mouse click into a selected Zone id.
    //
    // Zones are stored as flat axis-aligned rectangles in the XZ plane
    // (Zone::minX/maxX/minZ/maxZ, see Zone.h) — buildings extrude upward from
    // that footprint, but the zone grid itself has no height. That means
    // picking doesn't need real mesh/triangle intersection: unproject the
    // mouse into a world-space ray, intersect that ray with the y=0 ground
    // plane, and test which zone's XZ rectangle contains the hit point.
    // This deliberately skips per-building picking (clicking a specific
    // building) — the master spec's Phase 10 mock only ever inspects at
    // zone granularity, and per-triangle picking against CityMeshes would be
    // a lot of extra complexity for no additional decision-support value at
    // MVP scope.
    class Picking {
    public:
        // Unprojects the mouse position (in window/framebuffer pixel
        // coordinates, y-down as GLFW reports it) into a world-space ray
        // using the camera's current view/projection.
        static Ray ScreenPointToRay(double mouseX, double mouseY,
                                     int screenWidth, int screenHeight,
                                     const Camera& camera);

        // Intersects `ray` with the horizontal plane y == planeY. Returns
        // false if the ray is (near-)parallel to the plane, or if the
        // intersection lies behind the ray origin — both cases mean "the
        // click didn't land on the ground," e.g. the user is looking at the
        // sky.
        static bool IntersectGroundPlane(const Ray& ray, glm::vec3& outHitPoint,
                                          float planeY = 0.0f);

        // Returns the zone whose XZ bounds contain `groundPoint`, or nullptr
        // if the point falls outside every zone (e.g. clicked past the edge
        // of the study area). If zones ever overlap, the first match wins —
        // the Phase 2 zone grid is expected to be non-overlapping.
        static const Zone* PickZone(const std::vector<Zone>& zones,
                                     const glm::vec3& groundPoint);

        // Convenience wrapper combining the three steps above. Returns -1 if
        // nothing was hit (parallel/behind-camera ray, or no zone at the hit
        // point) so callers can treat -1 as "deselect" uniformly.
        static int PickZoneId(double mouseX, double mouseY,
                               int screenWidth, int screenHeight,
                               const Camera& camera,
                               const std::vector<Zone>& zones);
    };

}  // namespace twin
