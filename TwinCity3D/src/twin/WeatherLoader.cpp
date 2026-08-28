#include "WeatherLoader.h"
#include "../core/Log.h"
#include "../../external/json.hpp"

#include <fstream>

namespace twin {

    using json = nlohmann::json;

    bool WeatherLoader::Load(const std::string& path, WeatherData& out) {
        out = WeatherData{};
        out.valid = false;

        std::ifstream file(path);
        if (!file.is_open()) {
            LogWarn("WeatherLoader: could not open '" + path +
                "' — zone temperature stays a placeholder");
            return false;
        }

        json root;
        try {
            file >> root;
        }
        catch (const json::parse_error& e) {
            LogError(std::string("WeatherLoader: JSON parse error in '") + path + "': " + e.what());
            return false;
        }

        try {
            if (!root.contains("current")) {
                LogError("WeatherLoader: '" + path + "' is missing required \"current\" object");
                return false;
            }

            const auto& current = root.at("current");
            out.currentTemperature = current.value("temperature_c", 0.0);
            out.apparentTemperature = current.value("apparent_temperature_c",
                static_cast<double>(out.currentTemperature));
            out.humidity = current.value("humidity_pct", 0.0);
            out.precipitation = current.value("precipitation_mm", 0.0);
            out.windSpeed = current.value("wind_speed_kmh", 0.0);

            if (root.contains("hourly") && root.at("hourly").is_array()) {
                const auto& hourly = root.at("hourly");
                out.hourlyForecast.reserve(hourly.size());
                for (const auto& h : hourly) {
                    HourlyWeatherSample sample;
                    sample.time = h.value("time", std::string());
                    sample.temperature = h.value("temperature_c", 0.0);
                    out.hourlyForecast.push_back(sample);
                }
            }

            if (root.contains("_meta")) {
                const auto& meta = root.at("_meta");
                out.dataSource = meta.value("data_source", std::string("unknown"));
                out.fetchedAtUtc = meta.value("fetched_at_utc", std::string());
            }
            else {
                // Same rule Phase 2 follows for GIS data: never disguise
                // unlabeled data as real.
                out.dataSource = "unknown";
                LogWarn("WeatherLoader: '" + path + "' has no _meta.data_source — "
                    "treating provenance as unknown");
            }
        }
        catch (const json::exception& e) {
            LogError(std::string("WeatherLoader: malformed weather.json: ") + e.what());
            return false;
        }

        out.valid = true;

        LogInfo("WeatherLoader: loaded '" + path + "' [" + out.dataSource + "] current=" +
            std::to_string(out.currentTemperature) + "C, " +
            std::to_string(out.hourlyForecast.size()) + " hourly samples");

        return true;
    }

}  // namespace twin
