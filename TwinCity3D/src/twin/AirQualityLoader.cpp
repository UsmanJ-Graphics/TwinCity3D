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
            if (!root.contains("stations") || !root.at("stations").is_array()) {
                LogError("AirQualityLoader: '" + path + "' is missing required \"stations\" array");
                return false;
            }

            for (const auto& s : root.at("stations")) {
                AirQualitySample sample;
                sample.zoneId = s.value("zone_id", -1);
                sample.pm25 = s.value("pm25", 0.0);
                sample.aqi = s.value("aqi", 0.0);
                sample.stationId = s.value("station_id", std::string());
                // Same discipline GISLoader uses for buildings/green areas:
                // a sample with no resolvable zone_id can't be applied to
                // anything, so it's dropped here rather than silently
                // carried forward as dead weight.
                if (sample.zoneId >= 0) out.zones.push_back(std::move(sample));
            }

            if (out.zones.empty()) {
                LogWarn("AirQualityLoader: '" + path + "' parsed but contained no samples with a "
                    "valid zone_id -- zone air quality stays a placeholder");
                return false;
            }

            if (root.contains("_meta")) {
                const auto& meta = root.at("_meta");
                out.dataSource = meta.value("data_source", std::string("unknown"));
                out.fetchedAtUtc = meta.value("fetched_at_utc", std::string());
            }
            else {
                // Same rule WeatherLoader follows: never disguise
                // unlabeled data as real/live.
                out.dataSource = "unknown";
                LogWarn("AirQualityLoader: '" + path + "' has no _meta.data_source -- "
                    "treating provenance as unknown");
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
