#include "DigitalTwin.h"
#include "../gis/Triangulate.h"
#include "../core/Log.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace twin {

    using gis::PolygonSignedArea;

    namespace {
        // Phase 6 heat-burden weights. Kept as named constants (not magic
        // numbers) so they're easy to retune before Phase 8's heat-risk model
        // reuses the same idea with its own weights — see master spec's
        // "Keep the weights configurable" rule under Phase 8.
        constexpr float kWeightBuildingDensity = 0.5f;
        constexpr float kWeightGreenDeficit = 0.3f;
        constexpr float kWeightExposedSurface = 0.2f;

        // Total degrees C of spread applied across the full burden range
        // (i.e. a zone at burden 1.0 vs. a zone at burden 0.0, relative to the
        // study-area average, differ by up to this many degrees). Deliberately
        // small — this is a visualization aid, not a claimed measurement.
        constexpr float kHeatBurdenTemperatureSwingC = 3.0f;
    }

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

        ComputeEnvironmentalLayer();
    }

    void DigitalTwin::ComputeEnvironmentalLayer() {
        for (auto& zone : m_zones) {
            float greenDeficit = 1.0f - zone.greenCoverage;
            zone.exposedSurfaceRatio = std::clamp(
                1.0f - zone.buildingDensity - zone.greenCoverage, 0.0f, 1.0f);

            zone.environmentalHeatBurden = std::clamp(
                kWeightBuildingDensity * zone.buildingDensity +
                kWeightGreenDeficit * greenDeficit +
                kWeightExposedSurface * zone.exposedSurfaceRatio,
                0.0f, 1.0f);
        }

        float avgBurden = std::accumulate(m_zones.begin(), m_zones.end(), 0.0f,
            [](float sum, const Zone& z) { return sum + z.environmentalHeatBurden; }) /
            static_cast<float>(m_zones.size());

        LogInfo("DigitalTwin::ComputeEnvironmentalLayer: environmental heat burden computed "
            "on " + std::to_string(m_zones.size()) + " zones (study-area average burden=" +
            std::to_string(avgBurden) + ") — exposedSurfaceRatio/environmentalHeatBurden are "
            "derived from real Phase 3 geometry, not placeholders");
    }

    void DigitalTwin::ApplyWeather(const WeatherData& weather) {
        if (!weather.valid) {
            LogWarn("DigitalTwin::ApplyWeather: weather data is not valid — "
                "leaving zone temperatures as placeholders");
            return;
        }

        if (m_zones.empty()) {
            LogWarn("DigitalTwin::ApplyWeather: no zones to apply weather to");
            return;
        }

        float avgBurden = std::accumulate(m_zones.begin(), m_zones.end(), 0.0f,
            [](float sum, const Zone& z) { return sum + z.environmentalHeatBurden; }) /
            static_cast<float>(m_zones.size());

        float minTemp = 0.0f, maxTemp = 0.0f;
        for (size_t i = 0; i < m_zones.size(); ++i) {
            auto& zone = m_zones[i];
            // Phase 6: spread the single station reading across zones using
            // relative heat burden. A zone exactly at the average burden gets
            // the unmodified reading; hotter-burden zones read warmer,
            // greener/lower-burden zones read cooler. Still one weather
            // source — this is a modelled offset, not a second sensor.
            float offset = (zone.environmentalHeatBurden - avgBurden) * kHeatBurdenTemperatureSwingC;
            zone.temperature = weather.currentTemperature + offset;
            zone.temperatureIsPlaceholder = false;

            if (i == 0) { minTemp = maxTemp = zone.temperature; }
            else {
                minTemp = std::min(minTemp, zone.temperature);
                maxTemp = std::max(maxTemp, zone.temperature);
            }
        }

        LogInfo("DigitalTwin::ApplyWeather: base=" + std::to_string(weather.currentTemperature) +
            "C spread across " + std::to_string(m_zones.size()) + " zones via Phase 6 heat-burden "
            "offset [" + weather.dataSource + "] -> range " + std::to_string(minTemp) + "C to " +
            std::to_string(maxTemp) + "C" + (weather.IsLive() ? "" : " (NOT live — sample/fallback reading)") +
            " — MODELLED SPREAD, not per-zone sensor data");
    }

    void DigitalTwin::ApplyPopulation(const PopulationData& population) {
        if (!population.valid) {
            LogWarn("DigitalTwin::ApplyPopulation: population data is not valid — "
                "leaving zone population as placeholders");
            return;
        }

        int matched = 0;
        for (const auto& sample : population.zones) {
            Zone* zone = FindZone(sample.zoneId);
            if (!zone) continue;
            zone->population = sample.population;
            zone->populationDensity = sample.populationDensityPerKm2;
            zone->populationIsPlaceholder = false;
            ++matched;
        }

        int unmatched = static_cast<int>(population.zones.size()) - matched;
        if (unmatched > 0) {
            LogWarn("DigitalTwin::ApplyPopulation: " + std::to_string(unmatched) +
                " population.json entries referenced zone ids not present in this dataset");
        }

        LogInfo("DigitalTwin::ApplyPopulation: set population on " + std::to_string(matched) +
            "/" + std::to_string(m_zones.size()) + " zones [" + population.dataSource + "]" +
            (population.IsLive() ? "" : " (estimated — not a direct live measurement)") +
            ", study-area total=" + std::to_string(population.totalPopulation));
    }

    void DigitalTwin::ComputeHeatRisk(const HeatRiskWeights& weights) {
        if (m_zones.empty()) {
            LogWarn("DigitalTwin::ComputeHeatRisk: no zones to score");
            return;
        }

        HeatRiskModel::Compute(m_zones, weights);

        int scored = 0;
        for (const auto& z : m_zones) {
            if (!z.riskIsPlaceholder) ++scored;
        }

        LogInfo("DigitalTwin::ComputeHeatRisk: heat risk computed on " +
            std::to_string(scored) + "/" + std::to_string(m_zones.size()) +
            " zones (remainder still have placeholder temperature and/or population) "
            "— PROTOTYPE DECISION-SUPPORT SCORE, see HeatRiskModel");
    }

    void DigitalTwin::ComputePriority(const PriorityWeights& weights) {
        if (m_zones.empty()) {
            LogWarn("DigitalTwin::ComputePriority: no zones to rank");
            return;
        }

        PriorityModel::Compute(m_zones, weights);

        int ranked = 0;
        for (const auto& z : m_zones) {
            if (!z.priorityIsPlaceholder) ++ranked;
        }

        LogInfo("DigitalTwin::ComputePriority: priority computed on " +
            std::to_string(ranked) + "/" + std::to_string(m_zones.size()) +
            " zones (remainder still lack a real heat-risk score) "
            "— PROTOTYPE DECISION-SUPPORT PRIORITY, see PriorityModel");
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
