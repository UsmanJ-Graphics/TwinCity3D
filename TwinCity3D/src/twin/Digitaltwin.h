#pragma once
#include "Zone.h"
#include "WeatherData.h"
#include "../gis/GISTypes.h"
#include <vector>

namespace twin {

    // Owns the digital twin's zone grid and keeps each Zone's data-model fields
    // in sync with whatever real data is available. Phase 4: builds zones
    // from gis::ZoneBounds and fills in buildingDensity/greenCoverage from
    // Phase 3 geometry. Phase 5 adds ApplyWeather(), which clears the
    // temperature placeholder. Later phases (6-8, 11) call further Update*()
    // methods on this same class to fill in population, heatRisk, exposure,
    // and priority — the zone list itself doesn't change shape again.
    class DigitalTwin {
    public:
        // Builds one Zone per gis::ZoneBounds entry and computes the
        // geometry-derived fields (density, green coverage) from the dataset.
        void Build(const gis::GISDataset& dataset);

        // Phase 5, Step 3: applies current weather to every zone. MVP scope —
        // there's no per-zone spatial temperature model yet (Phase 6
        // differentiates zones using vegetation/built-up ratios), so every
        // zone takes the same study-area-wide currentTemperature. Does
        // nothing (leaves temperatureIsPlaceholder true on every zone) if
        // weather.valid is false, so a missing/broken weather.json can never
        // masquerade as a real reading.
        void ApplyWeather(const WeatherData& weather);

        const std::vector<Zone>& Zones() const { return m_zones; }
        std::vector<Zone>& Zones() { return m_zones; }

        // Returns nullptr if no zone with that id exists.
        Zone* FindZone(int zoneId);
        const Zone* FindZone(int zoneId) const;

    private:
        std::vector<Zone> m_zones;
    };

}  // namespace twin