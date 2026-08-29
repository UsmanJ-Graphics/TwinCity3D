#pragma once
#include <string>
#include <vector>

namespace twin {

    // Phase 26. One measured/estimated air-quality reading tied to a zone
    // (matched by zone_id when applied — same convention PopulationData /
    // SatelliteEnvironmentData use, NOT assumed to be index-aligned with
    // DigitalTwin's zone list).
    struct AirQualitySample {
        int zoneId{ -1 };
        float pm25{ 0.0f };     // micrograms per cubic meter (ug/m3)
        float aqi{ 0.0f };      // Air Quality Index (station-reported or estimated)
        std::string stationId;  // originating station/sensor id; empty if interpolated
    };

    // Phase 26. Mirrors WeatherData's real/sample labeling convention:
    // `valid` is false until AirQualityLoader has successfully parsed a real
    // file into this struct -- never treat a default-constructed
    // AirQualityData as real data.
    struct AirQualityData {
        std::vector<AirQualitySample> zones;

        std::string dataSource;    // "openaq_live" | "aqicn_live" | "sample_fallback" | "unknown"
        std::string fetchedAtUtc;  // ISO8601, from air_quality.json's _meta.fetched_at_utc

        bool valid = false;

        bool IsLive() const { return dataSource == "openaq_live" || dataSource == "aqicn_live"; }
    };

}  // namespace twin
