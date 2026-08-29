#pragma once
#include <string>
#include "AirQualityData.h"

namespace twin {

    // Phase 26. Reads data/processed/air_quality.json (written by an
    // external Python preprocessing step -- e.g. python/fetch_air_quality.py
    // pulling from OpenAQ/AQICN, mirroring how python/fetch_weather.py feeds
    // WeatherLoader) into an AirQualityData. Mirrors WeatherLoader's
    // contract exactly: never throws, returns false and leaves
    // out.valid == false on any failure (missing file, malformed JSON,
    // missing required fields, or a file with zero usable samples) -- a
    // missing/broken air_quality.json can never masquerade as real data
    // (Critical Engineering Rule #2/#3).
    class AirQualityLoader {
    public:
        static bool Load(const std::string& path, AirQualityData& out);
    };

}  // namespace twin
