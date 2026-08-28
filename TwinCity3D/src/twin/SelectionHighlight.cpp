#include "SelectionHighlight.h"

#include <algorithm>
#include <vector>

namespace twin {

    namespace {
        void AppendRect(std::vector<Vertex>& verts, std::vector<unsigned int>& indices,
                         float x0, float x1, float z0, float z1, float y) {
            if (x1 <= x0 || z1 <= z0) return;  // degenerate — nothing to draw

            const glm::vec3 up(0.0f, 1.0f, 0.0f);
            unsigned int base = static_cast<unsigned int>(verts.size());
            verts.push_back({glm::vec3(x0, y, z0), up, 0.0f});
            verts.push_back({glm::vec3(x1, y, z0), up, 0.0f});
            verts.push_back({glm::vec3(x1, y, z1), up, 0.0f});
            verts.push_back({glm::vec3(x0, y, z1), up, 0.0f});
            indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
        }
    }  // namespace

    Mesh SelectionHighlight::Build(const Zone& zone, float thickness, float y) {
        float ox0 = zone.minX, ox1 = zone.maxX, oz0 = zone.minZ, oz1 = zone.maxZ;

        // Clamp thickness so the inner rectangle never inverts on a small
        // zone — same reasoning CityMeshBuilder applies to building
        // heights defaulting rather than going negative/zero.
        float maxThickness = std::min(zone.Width(), zone.Depth()) * 0.33f;
        float t = std::min(thickness, std::max(maxThickness, 0.1f));

        float ix0 = ox0 + t, ix1 = ox1 - t;
        float iz0 = oz0 + t, iz1 = oz1 - t;

        std::vector<Vertex> verts;
        std::vector<unsigned int> indices;

        // Four axis-aligned bands tiling a rectangular ring — simpler than
        // mitered corners, and at frame-outline scale the difference isn't
        // visible. Top/bottom bands run the full outer width; left/right
        // bands fill only the middle strip so nothing overlaps.
        AppendRect(verts, indices, ox0, ox1, iz1, oz1, y);  // top
        AppendRect(verts, indices, ox0, ox1, oz0, iz0, y);  // bottom
        AppendRect(verts, indices, ox0, ix0, iz0, iz1, y);  // left
        AppendRect(verts, indices, ix1, ox1, iz0, iz1, y);  // right

        Mesh mesh;
        if (!verts.empty()) mesh.Upload(verts, indices);
        return mesh;
    }

}  // namespace twin
