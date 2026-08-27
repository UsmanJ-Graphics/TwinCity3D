#include "CityMeshBuilder.h"
#include "../gis/Triangulate.h"
#include "../core/Log.h"

namespace twin {

using gis::Building;
using gis::Facility;
using gis::GISDataset;
using gis::GreenArea;
using gis::Road;
using gis::TriangulatePolygon;

namespace {

// Outward wall normal for edge (p1 -> p2), disambiguated against the
// footprint centroid. We don't rely on consistent CW/CCW winding from the
// GIS pipeline (real OSM ways can go either way), so normals are derived
// geometrically rather than from triangle order — see Renderer.h note on
// why backface culling is left off for Phase 3.
glm::vec3 OutwardWallNormal(const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& centroid) {
    glm::vec2 edge = p2 - p1;
    glm::vec2 mid = (p1 + p2) * 0.5f;
    glm::vec2 candidateA(edge.y, -edge.x);
    glm::vec2 toCentroid = centroid - mid;
    // Pick whichever candidate points away from the centroid.
    glm::vec2 outward2D = (glm::dot(candidateA, toCentroid) > 0.0f) ? -candidateA : candidateA;
    if (glm::length(outward2D) > 1e-6f) outward2D = glm::normalize(outward2D);
    return glm::vec3(outward2D.x, 0.0f, outward2D.y);
}

glm::vec2 PolygonCentroid(const std::vector<glm::vec2>& poly) {
    glm::vec2 c(0.0f);
    for (const auto& p : poly) c += p;
    return poly.empty() ? c : c / static_cast<float>(poly.size());
}

void AppendQuad(std::vector<Vertex>& verts, std::vector<unsigned int>& indices,
                 const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d,
                 const glm::vec3& normal) {
    unsigned int base = static_cast<unsigned int>(verts.size());
    verts.push_back({a, normal, 0.0f});
    verts.push_back({b, normal, 0.0f});
    verts.push_back({c, normal, 0.0f});
    verts.push_back({d, normal, 0.0f});
    indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
}

void AppendBuildingGeometry(const Building& b, std::vector<Vertex>& verts, std::vector<unsigned int>& indices,
                             int& skipped) {
    const auto& footprint = b.footprint;
    if (footprint.size() < 3) { ++skipped; return; }

    std::vector<unsigned int> roofTris = TriangulatePolygon(footprint);
    if (roofTris.empty()) { ++skipped; return; }

    glm::vec2 centroid = (b.centroid != glm::vec2(0.0f)) ? b.centroid : PolygonCentroid(footprint);
    float h = b.height > 0.0f ? b.height : 3.0f;

    // Roof cap.
    unsigned int roofBase = static_cast<unsigned int>(verts.size());
    for (const auto& p : footprint) {
        verts.push_back({glm::vec3(p.x, h, p.y), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f});
    }
    for (unsigned int idx : roofTris) indices.push_back(roofBase + idx);

    // Walls, one outward-facing quad per edge.
    size_t n = footprint.size();
    for (size_t i = 0; i < n; ++i) {
        const glm::vec2& p1 = footprint[i];
        const glm::vec2& p2 = footprint[(i + 1) % n];
        glm::vec3 normal = OutwardWallNormal(p1, p2, centroid);

        glm::vec3 A(p1.x, 0.0f, p1.y);
        glm::vec3 Bv(p2.x, 0.0f, p2.y);
        glm::vec3 C(p2.x, h, p2.y);
        glm::vec3 D(p1.x, h, p1.y);
        AppendQuad(verts, indices, A, Bv, C, D, normal);
    }
}

void AppendRoadGeometry(const Road& r, std::vector<Vertex>& verts, std::vector<unsigned int>& indices) {
    const float y = 0.05f;  // lift slightly above ground grid to avoid z-fighting
    const float halfWidth = r.width * 0.5f;
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    for (size_t i = 0; i + 1 < r.polyline.size(); ++i) {
        glm::vec2 p1 = r.polyline[i];
        glm::vec2 p2 = r.polyline[i + 1];
        glm::vec2 dir = p2 - p1;
        if (glm::length(dir) < 1e-5f) continue;
        dir = glm::normalize(dir);
        glm::vec2 perp(-dir.y, dir.x);
        glm::vec2 offset = perp * halfWidth;

        glm::vec3 A(p1.x - offset.x, y, p1.y - offset.y);
        glm::vec3 Bv(p1.x + offset.x, y, p1.y + offset.y);
        glm::vec3 C(p2.x + offset.x, y, p2.y + offset.y);
        glm::vec3 D(p2.x - offset.x, y, p2.y - offset.y);
        AppendQuad(verts, indices, A, Bv, C, D, up);
    }
}

void AppendGreenAreaGeometry(const GreenArea& g, std::vector<Vertex>& verts, std::vector<unsigned int>& indices,
                              int& skipped) {
    if (g.polygon.size() < 3) { ++skipped; return; }
    std::vector<unsigned int> tris = TriangulatePolygon(g.polygon);
    if (tris.empty()) { ++skipped; return; }

    const float y = 0.03f;
    unsigned int base = static_cast<unsigned int>(verts.size());
    for (const auto& p : g.polygon) {
        verts.push_back({glm::vec3(p.x, y, p.y), glm::vec3(0.0f, 1.0f, 0.0f), 0.0f});
    }
    for (unsigned int idx : tris) indices.push_back(base + idx);
}

void AppendFacilityMarker(const Facility& f, std::vector<Vertex>& verts, std::vector<unsigned int>& indices) {
    const float halfWidth = 4.0f;
    const float baseY = 0.0f;
    const float topY = 18.0f;
    const glm::vec3 up(0.0f, 1.0f, 0.0f);

    glm::vec3 apex(f.position.x, topY, f.position.y);
    glm::vec3 b0(f.position.x - halfWidth, baseY, f.position.y - halfWidth);
    glm::vec3 b1(f.position.x + halfWidth, baseY, f.position.y - halfWidth);
    glm::vec3 b2(f.position.x + halfWidth, baseY, f.position.y + halfWidth);
    glm::vec3 b3(f.position.x - halfWidth, baseY, f.position.y + halfWidth);

    unsigned int base = static_cast<unsigned int>(verts.size());
    verts.push_back({apex, up, 0.0f});  // base + 0
    verts.push_back({b0, up, 0.0f});    // base + 1
    verts.push_back({b1, up, 0.0f});    // base + 2
    verts.push_back({b2, up, 0.0f});    // base + 3
    verts.push_back({b3, up, 0.0f});    // base + 4

    unsigned int apexIdx = base, i0 = base + 1, i1 = base + 2, i2 = base + 3, i3 = base + 4;
    indices.insert(indices.end(), {
        apexIdx, i0, i1,
        apexIdx, i1, i2,
        apexIdx, i2, i3,
        apexIdx, i3, i0,
    });
}

}  // namespace

CityMeshes CityMeshBuilder::Build(const gis::GISDataset& dataset) {
    CityMeshes result;

    std::vector<Vertex> buildingVerts, roadVerts, greenVerts, facilityVerts;
    std::vector<unsigned int> buildingIdx, roadIdx, greenIdx, facilityIdx;

    int skippedBuildings = 0;
    for (const auto& b : dataset.buildings) {
        AppendBuildingGeometry(b, buildingVerts, buildingIdx, skippedBuildings);
    }
    for (const auto& r : dataset.roads) {
        AppendRoadGeometry(r, roadVerts, roadIdx);
    }
    int skippedGreen = 0;
    for (const auto& g : dataset.greenAreas) {
        AppendGreenAreaGeometry(g, greenVerts, greenIdx, skippedGreen);
    }
    for (const auto& f : dataset.facilities) {
        AppendFacilityMarker(f, facilityVerts, facilityIdx);
    }

    if (!buildingVerts.empty()) result.buildings.Upload(buildingVerts, buildingIdx);
    if (!roadVerts.empty()) result.roads.Upload(roadVerts, roadIdx);
    if (!greenVerts.empty()) result.greenAreas.Upload(greenVerts, greenIdx);
    if (!facilityVerts.empty()) result.facilities.Upload(facilityVerts, facilityIdx);

    result.buildingCount = static_cast<int>(dataset.buildings.size()) - skippedBuildings;
    result.roadCount = static_cast<int>(dataset.roads.size());
    result.greenAreaCount = static_cast<int>(dataset.greenAreas.size()) - skippedGreen;
    result.facilityCount = static_cast<int>(dataset.facilities.size());
    result.skippedBuildingCount = skippedBuildings + skippedGreen;

    LogInfo("CityMeshBuilder: " + std::to_string(result.buildingCount) + " buildings, " +
            std::to_string(result.roadCount) + " roads, " +
            std::to_string(result.greenAreaCount) + " green areas, " +
            std::to_string(result.facilityCount) + " facilities" +
            (result.skippedBuildingCount > 0
                 ? (" (" + std::to_string(result.skippedBuildingCount) + " skipped: bad geometry)")
                 : ""));

    return result;
}

}  // namespace twin
