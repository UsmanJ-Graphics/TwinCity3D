#include "AirQualityLoader.h"
#include "../core/Log.h"
#include "../../external/json.hpp"

#include <fstream>

namespace twin {

    using json = nlohmann::json;

    bool AirQualityLoader::Load(const std::string& path, AirQualityData& out) {
        out = AirQualityData{};
        out.valid = false;

        std::ifstream file(path);
        if (!file.is_open()) {
            LogWarn("AirQualityLoader: could not open '" + path +
                "' -- zone air quality stays a placeholder");
            return false;
        }

        json root;
        try {
            file >> root;
        }
        catch (const json::parse_error& e) {
            LogError(std::string("AirQualityLoader: JSON parse error in '") + path + "': " + e.what());
            return false;
        }

        try {
            // Accept either a "stations" array (station records) or a
            // "zones" array (pre-aggregated per-zone readings) for
            // compatibility with different preprocessing pipelines.
            const auto samplesKey = root.contains("stations") && root["stations"].is_array()
                ? std::optional<std::string>("stations")
                : (root.contains("zones") && root["zones"].is_array() ? std::optional<std::string>("zones") : std::nullopt);

            if (!samplesKey.has_value()) {
                LogError("AirQualityLoader: '" + path + "' is missing required \"stations\" or \"zones\" array");
                return false;
            }

            for (const auto& s : root.at(*samplesKey)) {
                AirQualitySample sample;
                sample.zoneId = s.value("zone_id", -1);
                sample.pm25 = s.value("pm25", 0.0);
                sample.aqi = s.value("aqi", 0.0);
                sample.stationId = s.value("station_id", std::string());
                if (sample.zoneId >= 0) out.zones.push_back(std::move(sample));
            }

            if (out.zones.empty()) {
                LogWarn("AirQualityLoader: '" + path + "' parsed but contained no samples with a valid zone_id -- zone air quality stays a placeholder");
                return false;
            }

            // Prefer explicit metadata if present, otherwise accept a top-level data_source string.
            if (root.contains("_meta") && root["_meta"].is_object()) {
                const auto& meta = root.at("_meta");
                out.dataSource = meta.value("data_source", std::string("unknown"));
                out.fetchedAtUtc = meta.value("fetched_at_utc", std::string());
            } else if (root.contains("data_source") && root["data_source"].is_string()) {
                out.dataSource = root.value("data_source", std::string("unknown"));
            } else {
                out.dataSource = "unknown";
                LogWarn("AirQualityLoader: '" + path + "' has no _meta.data_source or data_source field -- treating provenance as unknown");
            }
        }
        catch (const json::exception& e) {
            LogError(std::string("AirQualityLoader: malformed air_quality.json: ") + e.what());
            return false;
        }

        out.valid = true;

        LogInfo("AirQualityLoader: loaded '" + path + "' [" + out.dataSource + "] " +
            std::to_string(out.zones.size()) + " station sample(s)");

        return true;
    }

}  // namespace twin
