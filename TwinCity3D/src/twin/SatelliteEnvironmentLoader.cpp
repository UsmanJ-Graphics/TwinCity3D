#include "SatelliteEnvironmentLoader.h"
#include "../core/Log.h"
#include "../../external/json.hpp"
#include <fstream>
namespace twin {
bool SatelliteEnvironmentLoader::Load(const std::string& path, SatelliteEnvironmentData& out) {
    out = SatelliteEnvironmentData{}; std::ifstream file(path); if (!file) return false;
    try {
        nlohmann::json root; file >> root;
        if (!root.contains("zones") || !root["zones"].is_array()) return false;
        for (const auto& z : root["zones"]) { SatelliteEnvironmentSample s; s.zoneId=z.value("zone_id",-1); s.vegetationIndex=z.value("vegetation_index",0.0f); s.builtUpIndex=z.value("built_up_index",0.0f); if(s.zoneId>=0) out.zones.push_back(s); }
        out.dataSource=root.value("data_source",std::string("unknown")); out.method=root.value("method",std::string()); out.valid=!out.zones.empty();
    } catch (const std::exception& e) { LogWarn(std::string("SatelliteEnvironmentLoader: ")+e.what()); return false; }
    LogInfo("SatelliteEnvironmentLoader: loaded " + std::to_string(out.zones.size()) + " zones [" + out.dataSource + "]"); return out.valid;
}}
