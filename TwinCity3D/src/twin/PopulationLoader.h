#pragma once
#include <string>
#include "PopulationData.h"

namespace twin {

    // Phase 7. Reads data/processed/population.json (written by
    // python/estimate_population.py) into a PopulationData. Mirrors
    // WeatherLoader's contract: never throws, returns false and leaves
    // out.valid == false on any failure (missing file, malformed JSON,
    // missing required fields, or a parsed-but-empty zone list).
    class PopulationLoader {
    public:
        static bool Load(const std::string& path, PopulationData& out);
    };

}  // namespace twin
