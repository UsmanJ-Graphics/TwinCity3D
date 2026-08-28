#include "PopulationLoader.h"
#include "../core/Log.h"
#include "../../external/json.hpp"

#include <fstream>

namespace twin {

    using json = nlohmann::json;

    bool PopulationLoader::Load(const std::string& path, PopulationData& out) {
        out = PopulationData{};
        out.valid = false;

        std::ifstream file(path);
        if (!file.is_open()) {
            LogWarn("PopulationLoader: could not open '" + path +
                "' — zone population stays a placeholder");
            return false;
        }

        json root;
        try {
            file >> root;
        }
        catch (const json::parse_error& e) {
            LogError(std::string("PopulationLoader: JSON parse error in '") + path + "': " + e.what());
            return false;
        }

        try {
            if (!root.contains("zones") || !root.at("zones").is_array()) {
                LogError("PopulationLoader: '" + path + "' is missing required \"zones\" array");
                return false;
            }

            for (const auto& z : root.at("zones")) {
                ZonePopulationSample sample;
                sample.zoneId = z.value("zone_id", -1);
                sample.population = z.value("population", 0);
                sample.populationDensityPerKm2 = z.value("population_density_per_km2", 0.0);
                if (sample.zoneId >= 0) out.zones.push_back(sample);
            }

            out.dataSource = root.value("data_source", std::string("unknown"));
            out.method = root.value("method", std::string());

            if (root.contains("_meta")) {
                const auto& meta = root.at("_meta");
                out.generatedAtUtc = meta.value("generated_at_utc", std::string());
                out.totalPopulation = meta.value("total_population", 0);
            }

            if (out.dataSource == "unknown") {
                // Same rule Phase 2/5 follow for GIS/weather data: never
                // disguise unlabeled data as real.
                LogWarn("PopulationLoader: '" + path + "' has no data_source — "
                    "treating provenance as unknown");
            }
        }
        catch (const json::exception& e) {
            LogError(std::string("PopulationLoader: malformed population.json: ") + e.what());
            return false;
        }

        if (out.zones.empty()) {
            LogWarn("PopulationLoader: '" + path + "' parsed but contained zero zone entries — "
                "zone population stays a placeholder");
            return false;
        }

        out.valid = true;

        LogInfo("PopulationLoader: loaded '" + path + "' [" + out.dataSource + "] " +
            std::to_string(out.zones.size()) + " zone entries, total_population=" +
            std::to_string(out.totalPopulation));

        return true;
    }

}  // namespace twin
