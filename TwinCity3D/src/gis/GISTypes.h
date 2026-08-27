#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace twin::gis {

// All positions here are in the digital twin's local Cartesian system
// (meters, XZ plane, Y is elevation) — see docs/data_sources.md for the
// lat/lon -> local conversion performed by the Python pipeline. This layer
// never sees lat/lon.

struct Building {
    std::string id;
    std::vector<glm::vec2> footprint;  // XZ polygon, CCW or CW (triangulator handles either)
    float height{6.0f};
    std::string heightSource;          // "osm_height_tag" | "estimated_from_levels" | "estimated_default"
    glm::vec2 centroid{0.0f};
    int zoneId{-1};
    std::string buildingType;
    bool sampleData{false};
};

struct Road {
    std::string id;
    std::vector<glm::vec2> polyline;
    float width{7.0f};
    std::string roadType;              // "primary" | "secondary" | "residential"
    bool sampleData{false};
};

struct GreenArea {
    std::string id;
    std::vector<glm::vec2> polygon;
    std::string greenType;             // "park" | "trees"
    glm::vec2 centroid{0.0f};
    int zoneId{-1};
    bool sampleData{false};
};

struct Facility {
    std::string id;
    std::string name;
    std::string facilityType;          // "school" | "hospital" | ...
    glm::vec2 position{0.0f};
    int zoneId{-1};
    bool sampleData{false};
};

struct ZoneBounds {
    int id{-1};
    float minX{0.0f}, maxX{0.0f}, minZ{0.0f}, maxZ{0.0f};
};

// Everything Phase 3 (and later phases) needs to build/inspect the city.
struct GISDataset {
    std::vector<Building> buildings;
    std::vector<Road> roads;
    std::vector<GreenArea> greenAreas;
    std::vector<Facility> facilities;
    std::vector<ZoneBounds> zones;

    // True if any loaded file was the synthetic sample_fallback dataset
    // rather than a live OSM extract (Critical Engineering Rule #2: never
    // present sample data as real — the UI layer uses this to badge it).
    bool isSampleData{false};
};

}  // namespace twin::gis
