#include "DigitalTwin.h"
#include "../gis/Triangulate.h"
#include "../core/Log.h"

#include <algorithm>
#include <cmath>

namespace twin {

    using gis::PolygonSignedArea;

    void DigitalTwin::Build(const gis::GISDataset& dataset) {
        m_zones.clear();
        m_zones.reserve(dataset.zones.size());

        for (const auto& zb : dataset.zones) {
            Zone zone;
            zone.id = zb.id;
            zone.minX = zb.minX;
            zone.maxX = zb.maxX;
            zone.minZ = zb.minZ;
            zone.maxZ = zb.maxZ;
            m_zones.push_back(zone);
        }

        if (m_zones.empty()) {
            LogWarn("DigitalTwin::Build: no zones in dataset (zones.json missing/empty) — "
                "density/coverage cannot be computed per zone");
            return;
        }

        // Accumulate footprint area per zone from each building's own zoneId
        // (assigned during preprocessing, Phase 2) rather than re-deriving it
        // from geometry here — keeps this class agnostic to how zoning works.
        std::vector<float> builtArea(m_zones.size(), 0.0f);
        std::vector<float> greenArea(m_zones.size(), 0.0f);

        auto zoneIndex = [this](int zoneId) -> int {
            for (size_t i = 0; i < m_zones.size(); ++i) {
                if (m_zones[i].id == zoneId) return static_cast<int>(i);
            }
            return -1;
            };

        int unassignedBuildings = 0;
        for (const auto& b : dataset.buildings) {
            int idx = zoneIndex(b.zoneId);
            if (idx < 0) { ++unassignedBuildings; continue; }
            builtArea[idx] += std::fabs(PolygonSignedArea(b.footprint));
        }

        int unassignedGreen = 0;
        for (const auto& g : dataset.greenAreas) {
            int idx = zoneIndex(g.zoneId);
            if (idx < 0) { ++unassignedGreen; continue; }
            greenArea[idx] += std::fabs(PolygonSignedArea(g.polygon));
        }

        for (size_t i = 0; i < m_zones.size(); ++i) {
            float area = m_zones[i].Area();
            if (area <= 0.0f) continue;
            // Clamp to [0,1]: overlapping/imprecise sample footprints could
            // otherwise push a fraction slightly past 1.0.
            m_zones[i].buildingDensity = std::clamp(builtArea[i] / area, 0.0f, 1.0f);
            m_zones[i].greenCoverage = std::clamp(greenArea[i] / area, 0.0f, 1.0f);
        }

        if (unassignedBuildings > 0 || unassignedGreen > 0) {
            LogWarn("DigitalTwin::Build: " + std::to_string(unassignedBuildings) +
                " buildings and " + std::to_string(unassignedGreen) +
                " green areas had no matching zone_id and were excluded from density/coverage");
        }

        LogInfo("DigitalTwin::Build: " + std::to_string(m_zones.size()) +
            " zones built with building density / green coverage from Phase 3 geometry "
            "(temperature/population/heatRisk still placeholders)");
    }

    void DigitalTwin::ApplyWeather(const WeatherData& weather) {
        if (!weather.valid) {
            LogWarn("DigitalTwin::ApplyWeather: weather data is not valid — "
                "leaving zone temperatures as placeholders");
            return;
        }

        for (auto& zone : m_zones) {
            zone.temperature = weather.currentTemperature;
            zone.temperatureIsPlaceholder = false;
        }

        LogInfo("DigitalTwin::ApplyWeather: set temperature=" +
            std::to_string(weather.currentTemperature) + "C on " +
            std::to_string(m_zones.size()) + " zones [" + weather.dataSource + "]" +
            (weather.IsLive() ? "" : " (NOT live — sample/fallback reading)"));
    }

    Zone* DigitalTwin::FindZone(int zoneId) {
        auto it = std::find_if(m_zones.begin(), m_zones.end(),
            [zoneId](const Zone& z) { return z.id == zoneId; });
        return it != m_zones.end() ? &(*it) : nullptr;
    }

    const Zone* DigitalTwin::FindZone(int zoneId) const {
        auto it = std::find_if(m_zones.begin(), m_zones.end(),
            [zoneId](const Zone& z) { return z.id == zoneId; });
        return it != m_zones.end() ? &(*it) : nullptr;
    }

}  // namespace twin