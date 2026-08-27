#include "GISLoader.h"
#include "../core/Log.h"

#include "../../external/json.hpp"
#include <fstream>
#include <sstream>

namespace twin::gis {

using json = nlohmann::json;

namespace {

bool ReadJsonFile(const std::string& path, json& out) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    try {
        out = json::parse(buffer.str());
    } catch (const std::exception& e) {
        LogError("Failed to parse JSON (" + path + "): " + e.what());
        return false;
    }
    return true;
}

std::vector<glm::vec2> ReadPointArray(const json& arr) {
    std::vector<glm::vec2> pts;
    pts.reserve(arr.size());
    for (const auto& p : arr) {
        if (p.size() >= 2) pts.emplace_back(p[0].get<float>(), p[1].get<float>());
    }
    return pts;
}

bool MarksSample(const json& doc, const json& record) {
    bool docLevel = doc.contains("data_source") && doc["data_source"].get<std::string>() == "sample_fallback";
    bool recordLevel = record.contains("sample_data") && record["sample_data"].get<bool>();
    return docLevel || recordLevel;
}

}  // namespace

bool GISLoader::Load(const std::string& dataDir, GISDataset& out) {
    const std::string sep = dataDir.empty() || dataDir.back() == '/' ? "" : "/";

    // --- buildings.json (required) ---
    json buildingsDoc;
    if (!ReadJsonFile(dataDir + sep + "buildings.json", buildingsDoc)) {
        LogError("GISLoader: buildings.json not found in " + dataDir);
        return false;
    }
    for (const auto& b : buildingsDoc.value("buildings", json::array())) {
        Building building;
        building.id = b.value("id", "");
        building.footprint = ReadPointArray(b.value("polygon", json::array()));
        building.height = b.value("height", 6.0f);
        building.heightSource = b.value("height_source", "estimated_default");
        if (b.contains("centroid") && b["centroid"].size() >= 2) {
            building.centroid = glm::vec2(b["centroid"][0].get<float>(), b["centroid"][1].get<float>());
        }
        building.zoneId = b.value("zone_id", -1);
        building.buildingType = b.value("building_type", "unknown");
        building.sampleData = MarksSample(buildingsDoc, b);
        if (building.sampleData) out.isSampleData = true;
        if (building.footprint.size() >= 3) out.buildings.push_back(std::move(building));
    }

    // --- roads.json (optional) ---
    json roadsDoc;
    if (ReadJsonFile(dataDir + sep + "roads.json", roadsDoc)) {
        for (const auto& r : roadsDoc.value("roads", json::array())) {
            Road road;
            road.id = r.value("id", "");
            road.polyline = ReadPointArray(r.value("polyline", json::array()));
            road.width = r.value("width", 7.0f);
            road.roadType = r.value("road_type", "residential");
            road.sampleData = MarksSample(roadsDoc, r);
            if (road.sampleData) out.isSampleData = true;
            if (road.polyline.size() >= 2) out.roads.push_back(std::move(road));
        }
    } else {
        LogWarn("GISLoader: roads.json not found, continuing without roads");
    }

    // --- green_areas.json (optional) ---
    json greenDoc;
    if (ReadJsonFile(dataDir + sep + "green_areas.json", greenDoc)) {
        for (const auto& g : greenDoc.value("green_areas", json::array())) {
            GreenArea green;
            green.id = g.value("id", "");
            green.polygon = ReadPointArray(g.value("polygon", json::array()));
            green.greenType = g.value("green_type", "park");
            if (g.contains("centroid") && g["centroid"].size() >= 2) {
                green.centroid = glm::vec2(g["centroid"][0].get<float>(), g["centroid"][1].get<float>());
            }
            green.zoneId = g.value("zone_id", -1);
            green.sampleData = MarksSample(greenDoc, g);
            if (green.sampleData) out.isSampleData = true;
            if (green.polygon.size() >= 3) out.greenAreas.push_back(std::move(green));
        }
    } else {
        LogWarn("GISLoader: green_areas.json not found, continuing without green areas");
    }

    // --- facilities.json (optional) ---
    json facilitiesDoc;
    if (ReadJsonFile(dataDir + sep + "facilities.json", facilitiesDoc)) {
        for (const auto& f : facilitiesDoc.value("facilities", json::array())) {
            Facility facility;
            facility.id = f.value("id", "");
            facility.name = f.value("name", "");
            facility.facilityType = f.value("facility_type", "unknown");
            if (f.contains("position") && f["position"].size() >= 2) {
                facility.position = glm::vec2(f["position"][0].get<float>(), f["position"][1].get<float>());
            }
            facility.zoneId = f.value("zone_id", -1);
            facility.sampleData = MarksSample(facilitiesDoc, f);
            if (facility.sampleData) out.isSampleData = true;
            out.facilities.push_back(std::move(facility));
        }
    } else {
        LogWarn("GISLoader: facilities.json not found, continuing without facilities");
    }

    // --- zones.json (optional, used from Phase 4 onward but load now) ---
    json zonesDoc;
    if (ReadJsonFile(dataDir + sep + "zones.json", zonesDoc)) {
        for (const auto& z : zonesDoc.value("zones", json::array())) {
            ZoneBounds zb;
            zb.id = z.value("id", -1);
            zb.minX = z.value("min_x", 0.0f);
            zb.maxX = z.value("max_x", 0.0f);
            zb.minZ = z.value("min_z", 0.0f);
            zb.maxZ = z.value("max_z", 0.0f);
            out.zones.push_back(zb);
        }
        if (MarksSample(zonesDoc, json::object())) out.isSampleData = true;
    } else {
        LogWarn("GISLoader: zones.json not found, continuing without zone grid");
    }

    LogInfo("GISLoader: loaded " + std::to_string(out.buildings.size()) + " buildings, " +
            std::to_string(out.roads.size()) + " roads, " +
            std::to_string(out.greenAreas.size()) + " green areas, " +
            std::to_string(out.facilities.size()) + " facilities" +
            (out.isSampleData ? "  [SAMPLE DATA]" : ""));

    return true;
}

}  // namespace twin::gis
