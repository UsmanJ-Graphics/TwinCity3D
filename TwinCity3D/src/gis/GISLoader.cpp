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
            }
            catch (const std::exception& e) {
                LogError("Failed to parse JSON (" + path + "): " + e.what());
                return false;
            }
            return true;
        }

        // Reads a string field safely. Unlike json::value(key, "literal"), which
        // deduces its template argument as const char* and hits a known nlohmann
        // bug (get<const char*>() dereferences a null internal pointer without
        // checking is_string() first -- an access violation, not a catchable
        // exception, whenever the field is JSON null or any non-string type), this
        // checks contains()/is_string() explicitly and falls back to defaultValue
        // for anything else, including an explicit JSON null.
        std::string SafeString(const json& j, const char* key, const std::string& defaultValue) {
            if (!j.contains(key) || !j[key].is_string()) return defaultValue;
            return j[key].get<std::string>();
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
            bool docLevel = SafeString(doc, "data_source", "") == "sample_fallback";
            bool recordLevel = record.value("sample_data", false);
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
            building.id = SafeString(b, "id", "");
            building.footprint = ReadPointArray(b.value("polygon", json::array()));
            building.height = b.value("height", 6.0f);
            building.heightSource = SafeString(b, "height_source", "estimated_default");
            if (b.contains("centroid") && b["centroid"].size() >= 2) {
                building.centroid = glm::vec2(b["centroid"][0].get<float>(), b["centroid"][1].get<float>());
            }
            building.zoneId = b.value("zone_id", -1);
            building.buildingType = SafeString(b, "building_type", "unknown");
            building.sampleData = MarksSample(buildingsDoc, b);
            if (building.sampleData) out.isSampleData = true;
            if (building.footprint.size() >= 3) out.buildings.push_back(std::move(building));
        }

        // --- roads.json (optional) ---
        json roadsDoc;
        if (ReadJsonFile(dataDir + sep + "roads.json", roadsDoc)) {
            for (const auto& r : roadsDoc.value("roads", json::array())) {
                Road road;
                road.id = SafeString(r, "id", "");
                road.polyline = ReadPointArray(r.value("polyline", json::array()));
                road.width = r.value("width", 7.0f);
                road.roadType = SafeString(r, "road_type", "residential");
                road.sampleData = MarksSample(roadsDoc, r);
                if (road.sampleData) out.isSampleData = true;
                if (road.polyline.size() >= 2) out.roads.push_back(std::move(road));
            }
        }
        else {
            LogWarn("GISLoader: roads.json not found, continuing without roads");
        }

        // --- green_areas.json (optional) ---
        json greenDoc;
        if (ReadJsonFile(dataDir + sep + "green_areas.json", greenDoc)) {
            for (const auto& g : greenDoc.value("green_areas", json::array())) {
                GreenArea green;
                green.id = SafeString(g, "id", "");
                green.polygon = ReadPointArray(g.value("polygon", json::array()));
                green.greenType = SafeString(g, "green_type", "park");
                if (g.contains("centroid") && g["centroid"].size() >= 2) {
                    green.centroid = glm::vec2(g["centroid"][0].get<float>(), g["centroid"][1].get<float>());
                }
                green.zoneId = g.value("zone_id", -1);
                green.sampleData = MarksSample(greenDoc, g);
                if (green.sampleData) out.isSampleData = true;
                if (green.polygon.size() >= 3) out.greenAreas.push_back(std::move(green));
            }
        }
        else {
            LogWarn("GISLoader: green_areas.json not found, continuing without green areas");
        }

        // --- facilities.json (optional) ---
        json facilitiesDoc;
        if (ReadJsonFile(dataDir + sep + "facilities.json", facilitiesDoc)) {
            for (const auto& f : facilitiesDoc.value("facilities", json::array())) {
                Facility facility;
                facility.id = SafeString(f, "id", "");
                facility.name = SafeString(f, "name", "");
                facility.facilityType = SafeString(f, "facility_type", "unknown");
                if (f.contains("position") && f["position"].size() >= 2) {
                    facility.position = glm::vec2(f["position"][0].get<float>(), f["position"][1].get<float>());
                }
                facility.zoneId = f.value("zone_id", -1);
                facility.sampleData = MarksSample(facilitiesDoc, f);
                if (facility.sampleData) out.isSampleData = true;
                out.facilities.push_back(std::move(facility));
            }
        }
        else {
            LogWarn("GISLoader: facilities.json not found, continuing without facilities");
        }

        // --- zones.json (optional, used from Phase 4 onward but load now) ---
        json zonesDoc;
        if (ReadJsonFile(dataDir + sep + "zones.json", zonesDoc)) {
            int malformedZones = 0;
            for (const auto& z : zonesDoc.value("zones", json::array())) {
                ZoneBounds zb;
                zb.id = z.value("id", -1);

                // The zone-grid generator writes bounds nested under
                // "bounds_local" with keys x_min/x_max/z_min/z_max — NOT
                // top-level min_x/max_x/min_z/max_z. Reading the wrong shape
                // used to silently succeed (json::value() just falls back to
                // 0.0f on a miss) and gave every zone identical zero-area
                // bounds, which is why buildingDensity/greenCoverage/heatRisk
                // came out identical across all 16 zones. Read the real
                // nested object and warn loudly if it's ever missing again.
                if (z.contains("bounds_local") && z["bounds_local"].is_object()) {
                    const auto& b = z["bounds_local"];
                    zb.minX = b.value("x_min", 0.0f);
                    zb.maxX = b.value("x_max", 0.0f);
                    zb.minZ = b.value("z_min", 0.0f);
                    zb.maxZ = b.value("z_max", 0.0f);
                }
                else {
                    ++malformedZones;
                }

                if (zb.maxX <= zb.minX || zb.maxZ <= zb.minZ) {
                    LogWarn("GISLoader: zone id=" + std::to_string(zb.id) +
                        " has zero/negative area after parsing bounds_local "
                        "(minX=" + std::to_string(zb.minX) + " maxX=" + std::to_string(zb.maxX) +
                        " minZ=" + std::to_string(zb.minZ) + " maxZ=" + std::to_string(zb.maxZ) +
                        ") — density/coverage/heat-risk will stay placeholder for this zone");
                }

                out.zones.push_back(zb);
            }
            if (malformedZones > 0) {
                LogError("GISLoader: " + std::to_string(malformedZones) +
                    " zone(s) in zones.json were missing a \"bounds_local\" object entirely "
                    "— those zones defaulted to zero-area bounds");
            }
            if (MarksSample(zonesDoc, json::object())) out.isSampleData = true;
        }
        else {
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