#pragma once
#include <glm/glm.hpp>
#include <algorithm>
#include <vector>

// Minimal ear-clipping triangulator for simple 2D polygons (no holes).
// GIS building footprints are almost always convex or mildly non-convex
// (L-shapes, small notches), so ear-clipping is sufficient — no need for a
// full constrained Delaunay library for an MVP.
namespace twin::gis {

inline float PolygonSignedArea(const std::vector<glm::vec2>& poly) {
    float area = 0.0f;
    size_t n = poly.size();
    for (size_t i = 0; i < n; ++i) {
        const glm::vec2& a = poly[i];
        const glm::vec2& b = poly[(i + 1) % n];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5f;
}

inline bool PointInTriangle(const glm::vec2& p, const glm::vec2& a,
                             const glm::vec2& b, const glm::vec2& c) {
    float d1 = (p.x - b.x) * (a.y - b.y) - (a.x - b.x) * (p.y - b.y);
    float d2 = (p.x - c.x) * (b.y - c.y) - (b.x - c.x) * (p.y - c.y);
    float d3 = (p.x - a.x) * (c.y - a.y) - (c.x - a.x) * (p.y - a.y);
    bool hasNeg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool hasPos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(hasNeg && hasPos);
}

// Returns triangle indices (into `polygon`) for a simple polygon.
// Degenerate/unusable polygons (fewer than 3 valid vertices) return empty.
inline std::vector<unsigned int> TriangulatePolygon(const std::vector<glm::vec2>& polygon) {
    std::vector<unsigned int> result;
    if (polygon.size() < 3) return result;

    std::vector<unsigned int> indices(polygon.size());
    for (unsigned int i = 0; i < polygon.size(); ++i) indices[i] = i;

    // Ear clipping expects CCW winding.
    if (PolygonSignedArea(polygon) < 0.0f) {
        std::reverse(indices.begin(), indices.end());
    }

    int guard = 0;
    const int maxIterations = static_cast<int>(polygon.size()) * static_cast<int>(polygon.size()) + 8;

    while (indices.size() > 3 && guard++ < maxIterations) {
        bool earFound = false;
        size_t n = indices.size();

        for (size_t i = 0; i < n; ++i) {
            unsigned int iPrev = indices[(i + n - 1) % n];
            unsigned int iCurr = indices[i];
            unsigned int iNext = indices[(i + 1) % n];

            const glm::vec2& a = polygon[iPrev];
            const glm::vec2& b = polygon[iCurr];
            const glm::vec2& c = polygon[iNext];

            // Convexity check (CCW winding => positive cross product at an ear tip).
            float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (cross <= 0.0f) continue;

            bool anyPointInside = false;
            for (size_t j = 0; j < n; ++j) {
                unsigned int vj = indices[j];
                if (vj == iPrev || vj == iCurr || vj == iNext) continue;
                if (PointInTriangle(polygon[vj], a, b, c)) {
                    anyPointInside = true;
                    break;
                }
            }
            if (anyPointInside) continue;

            result.push_back(iPrev);
            result.push_back(iCurr);
            result.push_back(iNext);
            indices.erase(indices.begin() + i);
            earFound = true;
            break;
        }

        if (!earFound) break;  // degenerate polygon; keep whatever triangles we found
    }

    if (indices.size() == 3) {
        result.push_back(indices[0]);
        result.push_back(indices[1]);
        result.push_back(indices[2]);
    }

    return result;
}

}  // namespace twin::gis
