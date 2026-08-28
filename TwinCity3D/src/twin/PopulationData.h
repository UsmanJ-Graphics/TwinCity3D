#pragma once
#include <string>
#include <vector>

namespace twin {

    // One zone's slice of Phase 7's population.json (python/estimate_population.py).
    struct ZonePopulationSample {
        int zoneId{ -1 };
        int population{ 0 };
        float populationDensityPerKm2{ 0.0f };
    };

    // Phase 7. Mirrors WeatherData's real/estimated labeling convention:
    // `valid` is false until PopulationLoader has successfully parsed a real
    // file into this struct — never treat a default-constructed
    // PopulationData as real data. dataSource distinguishes:
    //   "worldpop_live_disaggregated" -> the study-area TOTAL is a real
    //                                    WorldPop figure; the per-zone split
    //                                    is still a modelled estimate (see
    //                                    estimate_population.py)
    //   "estimated_model"             -> nothing here is a live measurement;
    //                                    every number is a heuristic estimate
    //                                    from building floor area/occupancy
    //   "unknown"                     -> file present but unlabeled
    struct PopulationData {
        std::vector<ZonePopulationSample> zones;

        std::string dataSource;   // "worldpop_live_disaggregated" | "estimated_model" | "unknown"
        std::string method;       // e.g. "building_floor_area_occupancy_model"
        std::string generatedAtUtc;
        int totalPopulation{ 0 };

        bool valid = false;

        bool IsLive() const { return dataSource == "worldpop_live_disaggregated"; }
    };

}  // namespace twin
