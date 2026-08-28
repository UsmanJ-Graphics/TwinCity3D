#pragma once
#include <string>
#include <vector>

namespace twin {

    struct HourlyWeatherSample {
        std::string time;         // e.g. "2026-08-28T14:00", from Open-Meteo or the fallback generator
        float temperature = 0.0f; // degrees C
    };

    // Phase 5, Step 2. Mirrors gis::GISDataset's real/sample labeling
    // convention: `valid` is false until WeatherLoader has successfully
    // parsed a real file into this struct — never treat a default-constructed
    // WeatherData as real data.
    struct WeatherData {
        float currentTemperature = 0.0f;
        float apparentTemperature = 0.0f;
        float humidity = 0.0f;      // percent, 0-100
        float precipitation = 0.0f; // mm
        float windSpeed = 0.0f;     // km/h

        std::vector<HourlyWeatherSample> hourlyForecast; // ~24 samples

        std::string dataSource;   // "open_meteo_live" | "sample_fallback" | "unknown"
        std::string fetchedAtUtc; // ISO8601, from weather.json's _meta.fetched_at_utc

        bool valid = false;

        bool IsLive() const { return dataSource == "open_meteo_live"; }
    };

}  // namespace twin
