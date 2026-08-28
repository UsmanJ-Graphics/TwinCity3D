#pragma once
#include <string>
#include "WeatherData.h"

namespace twin {

    // Phase 5, Step 2. Reads data/processed/weather.json (written by
    // python/fetch_weather.py) into a WeatherData. Mirrors gis::GISLoader's
    // contract: never throws, returns false and leaves out.valid == false on
    // any failure (missing file, malformed JSON, missing required fields).
    class WeatherLoader {
    public:
        static bool Load(const std::string& path, WeatherData& out);
    };

}  // namespace twin
