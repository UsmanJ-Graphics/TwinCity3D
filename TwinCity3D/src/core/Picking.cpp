#include "Picking.h"

#include <cmath>
#include <glm/gtc/matrix_inverse.hpp>

namespace twin {

    Ray Picking::ScreenPointToRay(double mouseX, double mouseY,
                                   int screenWidth, int screenHeight,
                                   const Camera& camera) {
        Ray ray;
        ray.origin = camera.GetPosition();

        if (screenWidth <= 0 || screenHeight <= 0) {
            // Degenerate framebuffer size (e.g. minimized window) — return a
            // ray straight out of the camera so callers still get something
            // sane instead of dividing by zero below.
            return ray;
        }

        // Pixel -> NDC. GLFW reports mouseY with origin at the top of the
        // window, so it's flipped to match OpenGL's bottom-up NDC y-axis.
        float ndcX = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(screenWidth) - 1.0f;
        float ndcY = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(screenHeight);

        float aspectRatio = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);
        glm::mat4 projection = camera.GetProjectionMatrix(aspectRatio);
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 invViewProj = glm::inverse(projection * view);

        // Unproject the near and far points of the pixel's ray in one go
        // rather than trying to invert the projection analytically — simplest
        // correct approach for a perspective camera.
        glm::vec4 nearPointNdc(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPointNdc(ndcX, ndcY, 1.0f, 1.0f);

        glm::vec4 nearPointWorld = invViewProj * nearPointNdc;
        glm::vec4 farPointWorld = invViewProj * farPointNdc;

        if (nearPointWorld.w != 0.0f) nearPointWorld /= nearPointWorld.w;
        if (farPointWorld.w != 0.0f) farPointWorld /= farPointWorld.w;

        glm::vec3 nearPoint(nearPointWorld);
        glm::vec3 farPoint(farPointWorld);

        glm::vec3 direction = farPoint - nearPoint;
        float length = glm::length(direction);
        if (length > 1e-6f) {
            ray.direction = direction / length;
        }
        // ray.origin is the camera position, not `nearPoint` — using the
        // camera's actual eye point avoids any near-plane offset creeping
        // into distance calculations elsewhere.
        return ray;
    }

    bool Picking::IntersectGroundPlane(const Ray& ray, glm::vec3& outHitPoint, float planeY) {
        // Plane: y = planeY. Ray: origin + t * direction. Solve for t.
        const float kParallelEpsilon = 1e-5f;
        if (std::fabs(ray.direction.y) < kParallelEpsilon) {
            // Looking (near-)horizontally: the ray never crosses the ground
            // plane at a well-defined finite point.
            return false;
        }

        float t = (planeY - ray.origin.y) / ray.direction.y;
        if (t < 0.0f) {
            // Plane is behind the camera (e.g. camera below ground looking
            // further down, or looking up away from the ground).
            return false;
        }

        outHitPoint = ray.origin + t * ray.direction;
        return true;
    }

    const Zone* Picking::PickZone(const std::vector<Zone>& zones, const glm::vec3& groundPoint) {
        for (const auto& zone : zones) {
            if (groundPoint.x >= zone.minX && groundPoint.x <= zone.maxX &&
                groundPoint.z >= zone.minZ && groundPoint.z <= zone.maxZ) {
                return &zone;
            }
        }
        return nullptr;
    }

    int Picking::PickZoneId(double mouseX, double mouseY,
                             int screenWidth, int screenHeight,
                             const Camera& camera,
                             const std::vector<Zone>& zones) {
        Ray ray = ScreenPointToRay(mouseX, mouseY, screenWidth, screenHeight, camera);

        glm::vec3 hitPoint;
        if (!IntersectGroundPlane(ray, hitPoint)) {
            return -1;
        }

        const Zone* zone = PickZone(zones, hitPoint);
        return zone ? zone->id : -1;
    }

}  // namespace twin
