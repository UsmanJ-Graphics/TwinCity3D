#include "DigitalTwin.h"
#include "../gis/Triangulate.h"
#include "../core/Log.h"
#include "CitizenReport.h"
#include "../../external/json.hpp"
#include <fstream>
#include <ctime>

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

        // Phase 26. Weights for the CONCEPTUAL Combined Environmental Burden
        // overlay (see ComputeCombinedEnvironmentalBurden() below).
        // Deliberately kept separate from HeatRiskWeights/PriorityWeights —
        // never retune these to influence the core heat-risk or priority
        // score; this overlay is display-only.
        constexpr float kWeightCombinedHeat = 0.6f;
        constexpr float kWeightCombinedAirQuality = 0.4f;
    }

    bool DigitalTwin::UpdateReportStatus(int reportId, ReportStatus newStatus) {
        for (auto& r : m_reports) {
            if (r.id == reportId) {
                r.status = newStatus;
                LogInfo("DigitalTwin::UpdateReportStatus: report " + std::to_string(reportId) + " -> " + std::to_string(static_cast<int>(newStatus)));
                return true;
            }
        }
        return false;
    }

    void DigitalTwin::ApplyAirQuality(const AirQualityData& air) {
        if (!air.valid) {
            LogWarn("DigitalTwin::ApplyAirQuality: air quality data is not valid — leaving air quality placeholders");
            return;
        }

        // Map samples to zones by zoneId. Compute min/max pm25 across matched zones to normalize.
        float minPm = std::numeric_limits<float>::max();
        float maxPm = std::numeric_limits<float>::lowest();
        int matched = 0;

        for (const auto& s : air.zones) {
            Zone* z = FindZone(s.zoneId);
            if (!z) continue;
            z->pm25 = s.pm25;
            z->aqi = s.aqi;
            z->airQualityIsPlaceholder = false;
            minPm = std::min(minPm, z->pm25);
            maxPm = std::max(maxPm, z->pm25);
            ++matched;
        }

        if (matched == 0) {
            LogWarn("DigitalTwin::ApplyAirQuality: no samples matched any zone ids");
            return;
        }

        float range = maxPm - minPm;
        for (auto& z : m_zones) {
            if (z.airQualityIsPlaceholder) continue;
            if (range <= 0.0f) z.airQualityIndex = 0.5f;
            else z.airQualityIndex = std::clamp((z.pm25 - minPm) / range, 0.0f, 1.0f);
        }

        LogInfo("DigitalTwin::ApplyAirQuality: applied air quality to " + std::to_string(matched) + " zones [" + air.dataSource + "]");
    }

    void DigitalTwin::ComputeCombinedEnvironmentalBurden() {
        // Phase 26. Deliberately independent of HeatRiskModel/PriorityModel:
        // this reads zone.heatRisk and zone.airQualityIndex but never writes
        // back to either, and neither of those models ever reads this field.
        // A zone missing either input (no real heat-risk score, or no real
        // air-quality sample) is left with combinedBurdenIsPlaceholder ==
        // true rather than fabricating a partial score — same "no placeholder
        // masquerading as real data" discipline every other model in this
        // class follows.
        int scored = 0;
        for (auto& zone : m_zones) {
            if (zone.riskIsPlaceholder || zone.airQualityIsPlaceholder) {
                zone.combinedBurdenIsPlaceholder = true;
                continue;
            }

            float heatComponent = std::clamp(zone.heatRisk / 100.0f, 0.0f, 1.0f);
            float airComponent = std::clamp(zone.airQualityIndex, 0.0f, 1.0f);

            zone.combinedEnvironmentalBurden = std::clamp(
                (kWeightCombinedHeat * heatComponent +
                    kWeightCombinedAirQuality * airComponent) * 100.0f,
                0.0f, 100.0f);
            zone.combinedBurdenIsPlaceholder = false;
            ++scored;
        }

        LogInfo("DigitalTwin::ComputeCombinedEnvironmentalBurden: computed on " +
            std::to_string(scored) + "/" + std::to_string(m_zones.size()) +
            " zones (requires both real heat risk AND real air quality data) — "
            "CONCEPTUAL OVERLAY, deliberately separate from the core heat-risk "
            "model, never fed back into HeatRiskModel/PriorityModel");
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

    void DigitalTwin::ApplyTemperatureOffset(float deltaC) {
        if (m_zones.empty() || std::fabs(deltaC) < 1e-6f) return;

        int changed = 0;
        for (auto& zone : m_zones) {
            if (zone.temperatureIsPlaceholder) continue;
            zone.temperature += deltaC;
            ++changed;
        }

        LogInfo("DigitalTwin::ApplyTemperatureOffset: applied +" + std::to_string(deltaC) +
            "C HEATWAVE SCENARIO offset to " + std::to_string(changed) +
            " zones — MODELLED SCENARIO, not an observed temperature");
    }

    void DigitalTwin::ComputeFloodRisk(float rainfallMm) {
        // A 5 mm event is negligible; 80 mm is the requested heavy-rainfall
        // prototype reference. Spatial variation uses only OSM surface
        // proxies, not asserted terrain, drainage, or water data.
        const float stormIntensity = std::clamp((rainfallMm - 5.0f) / 75.0f, 0.0f, 1.0f);
        for (auto& zone : m_zones) {
            const float imperviousProxy = std::clamp(
                zone.buildingDensity + 0.6f * zone.exposedSurfaceRatio, 0.0f, 1.0f);
            const float surfaceSusceptibility = std::clamp(
                0.55f * imperviousProxy + 0.45f * (1.0f - zone.greenCoverage), 0.0f, 1.0f);
            zone.floodRisk = 100.0f * stormIntensity * (0.45f + 0.55f * surfaceSusceptibility);
            zone.floodRiskClass = zone.floodRisk < 25.0f ? "Low" :
                (zone.floodRisk < 50.0f ? "Moderate" : (zone.floodRisk < 75.0f ? "High" : "Critical"));
            zone.floodRiskIsPlaceholder = false;
        }
        LogInfo("DigitalTwin::ComputeFloodRisk: rainfall=" + std::to_string(rainfallMm) +
            " mm; OSM surface-susceptibility proxy — PROTOTYPE SCENARIO ESTIMATE, not a forecast");
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

    void DigitalTwin::ApplySatelliteEnvironment(const SatelliteEnvironmentData& environment) {
        if (!environment.valid) { LogWarn("DigitalTwin::ApplySatelliteEnvironment: no usable environmental layer"); return; }
        int matched = 0;
        for (const auto& sample : environment.zones) {
            Zone* zone = FindZone(sample.zoneId); if (!zone) continue;
            zone->vegetationIndex = std::clamp(sample.vegetationIndex, 0.0f, 1.0f);
            zone->builtUpIndex = std::clamp(sample.builtUpIndex, 0.0f, 1.0f);
            zone->satelliteIsPlaceholder = false; ++matched;
        }
        LogInfo("DigitalTwin::ApplySatelliteEnvironment: applied " + std::to_string(matched) + " zones [" + environment.dataSource + "]" + (environment.IsSatelliteProcessed() ? "" : " (OSM environmental proxy, not satellite)"));
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

    void DigitalTwin::ComputePopulationExposure() {
        int maxExposed = 0;
        for (auto& zone : m_zones) {
            if (zone.riskIsPlaceholder || zone.populationIsPlaceholder) {
                zone.populationExposureIsPlaceholder = true;
                continue;
            }
            zone.heatExposedPopulation = static_cast<int>(std::lround(
                static_cast<float>(zone.population) * std::clamp(zone.heatRisk / 100.0f, 0.0f, 1.0f)));
            zone.highRiskPopulation = zone.heatRisk >= 70.0f ? zone.population : 0;
            zone.extremeRiskPopulation = zone.heatRisk >= 85.0f ? zone.population : 0;
            zone.populationExposureIsPlaceholder = false;
            maxExposed = std::max(maxExposed, zone.heatExposedPopulation);
        }

        int totalExposed = 0, highRisk = 0, extremeRisk = 0;
        for (auto& zone : m_zones) {
            if (zone.populationExposureIsPlaceholder) continue;
            zone.heatExposureScore = maxExposed > 0
                ? static_cast<float>(zone.heatExposedPopulation) / static_cast<float>(maxExposed)
                : 0.0f;
            totalExposed += zone.heatExposedPopulation;
            highRisk += zone.highRiskPopulation;
            extremeRisk += zone.extremeRiskPopulation;
        }
        LogInfo("DigitalTwin::ComputePopulationExposure: heat-exposed=" + std::to_string(totalExposed) +
            ", high-risk=" + std::to_string(highRisk) + ", extreme-risk=" + std::to_string(extremeRisk) +
            " — MODELLED POPULATION EXPOSURE, not observed impact counts");
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

    void DigitalTwin::ComputeGreenInfrastructure(
        const GreenPriorityWeights& weights,
        float targetGreenCoverage) {

        if (m_zones.empty()) {
            LogWarn("DigitalTwin::ComputeGreenInfrastructure: no zones to analyse");
            return;
        }

        GreenInfrastructureModel::Compute(m_zones, weights, targetGreenCoverage);

        int scored = 0;
        for (const auto& z : m_zones) {
            if (!z.greenInfraIsPlaceholder) ++scored;
        }

        LogInfo("DigitalTwin::ComputeGreenInfrastructure: green infrastructure "
            "analysed on " + std::to_string(scored) + "/" +
            std::to_string(m_zones.size()) + " zones "
            "— PROTOTYPE GREEN INFRASTRUCTURE INDICATOR, see GreenInfrastructureModel");
    }

    void DigitalTwin::SaveBaselineMetrics() {
        for (auto& zone : m_zones) {
            zone.baselineHeatRisk = zone.heatRisk;
            zone.baselinePriority = zone.priority;
            // Only capture baseline green coverage if not already recorded
            if (zone.baselineGreenCoverage <= 0.0f) {
                zone.baselineGreenCoverage = zone.greenCoverage;
            }
        }
        LogInfo("DigitalTwin::SaveBaselineMetrics: baseline metrics saved for " +
            std::to_string(m_zones.size()) + " zones");
    }

    void DigitalTwin::ApplyInterventionScenario(float vegDeltaPct, float shadeDeltaPct, int targetZoneId) {
        if (m_zones.empty()) return;

        // Apply vegetation increase & shade cooling effect
        int modifiedCount = 0;
        for (auto& zone : m_zones) {
            if (targetZoneId >= 0 && zone.id != targetZoneId) continue;

            // Restore baseline green coverage first, then apply vegetation delta
            if (zone.baselineGreenCoverage > 0.0f) {
                zone.greenCoverage = std::clamp(zone.baselineGreenCoverage + vegDeltaPct, 0.0f, 1.0f);
            }
            else {
                zone.baselineGreenCoverage = zone.greenCoverage;
                zone.greenCoverage = std::clamp(zone.greenCoverage + vegDeltaPct, 0.0f, 1.0f);
            }

            // Shade cooling effect: shade reduces solar heat gain, effectively cooling local temperature
            // ~1.5°C reduction per 10% shade coverage in modelled urban microclimate proxy
            if (shadeDeltaPct > 0.0f && !zone.temperatureIsPlaceholder) {
                float shadeCoolingC = shadeDeltaPct * 15.0f; // e.g. +10% shade -> -1.5°C cooling offset
                zone.temperature = std::max(15.0f, zone.temperature - shadeCoolingC);
            }
            ++modifiedCount;
        }

        // Recompute environmental layer, risk, population exposure, priority, green infrastructure, and ecological impact
        ComputeEnvironmentalLayer();
        ComputeHeatRisk();
        ComputePopulationExposure();
        ComputePriority();
        ComputeGreenInfrastructure();
        ComputeEcologicalImpact();

        LogInfo("DigitalTwin::ApplyInterventionScenario: applied intervention (veg: +" +
            std::to_string(static_cast<int>(vegDeltaPct * 100.0f)) + "%, shade: +" +
            std::to_string(static_cast<int>(shadeDeltaPct * 100.0f)) + "%) to " +
            std::to_string(modifiedCount) + " zone(s) — MODELLED SCENARIO ESTIMATE");
    }

    void DigitalTwin::ComputeEcologicalImpact() {
        for (auto& zone : m_zones) {
            // Light pollution index (0..1): high building density and paved surface generate high night light intensity
            zone.lightPollutionIndex = std::clamp(zone.buildingDensity * 0.70f + zone.builtUpIndex * 0.30f, 0.0f, 1.0f);

            // Bird & Ecological Disturbance (0..100): light pollution + high building density impacting urban bird habitats
            float habitatSensitivity = zone.greenCoverage > 0.05f ? 1.2f : 0.8f;
            zone.birdEcologicalDisturbance = std::clamp(
                (zone.lightPollutionIndex * 0.55f + zone.buildingDensity * 0.30f + (1.0f - zone.greenCoverage) * 0.15f) * habitatSensitivity * 100.0f,
                0.0f, 100.0f);
            zone.ecologicalIsPlaceholder = false;
        }
        LogInfo("DigitalTwin::ComputeEcologicalImpact: light pollution and bird ecological disturbance computed for " +
            std::to_string(m_zones.size()) + " zones");
    }

    Zone* DigitalTwin::FindZone(int zoneId) {
        auto it = std::find_if(m_zones.begin(), m_zones.end(),
            [zoneId](const Zone& z) { return z.id == zoneId; });
        return it != m_zones.end() ? &(*it) : nullptr;
    }

    void DigitalTwin::AddReport(const CitizenReport& r) {
        CitizenReport rep = r;
        // assign id
        int maxId = -1;
        for (const auto& ex : m_reports) maxId = std::max(maxId, ex.id);
        rep.id = maxId + 1;

        // if zoneId present, set local coords to zone centroid
        if (rep.zoneId >= 0) {
            Zone* z = FindZone(rep.zoneId);
            if (z) {
                rep.localX = (z->minX + z->maxX) * 0.5f;
                rep.localZ = (z->minZ + z->maxZ) * 0.5f;
            }
        }

        m_reports.push_back(rep);
        LogInfo("DigitalTwin::AddReport: added report id=" + std::to_string(rep.id));
    }

    void DigitalTwin::SaveReports(const std::string& path) const {
        try {
            nlohmann::json root;
            root["reports"] = nlohmann::json::array();
            for (const auto& r : m_reports) {
                nlohmann::json jr;
                jr["id"] = r.id;
                jr["zone_id"] = r.zoneId;
                jr["local_x"] = r.localX;
                jr["local_z"] = r.localZ;
                jr["category"] = static_cast<int>(r.category);
                jr["status"] = static_cast<int>(r.status);
                jr["timestamp"] = r.timestamp;
                jr["description"] = r.description;
                root["reports"].push_back(jr);
            }
            std::ofstream out(path);
            out << root.dump(2);
            LogInfo("DigitalTwin::SaveReports: wrote " + path);
        } catch (const std::exception& e) {
            LogWarn(std::string("DigitalTwin::SaveReports: failed to write ") + path + ": " + e.what());
        }
    }

    void DigitalTwin::LoadReports(const std::string& path) {
        m_reports.clear();
        std::ifstream file(path);
        if (!file) { LogInfo("DigitalTwin::LoadReports: no file " + path + " found — starting with zero reports"); return; }
        try {
            nlohmann::json root; file >> root;
            if (!root.contains("reports") || !root["reports"].is_array()) return;
            for (const auto& jr : root["reports"]) {
                CitizenReport r;
                r.id = jr.value("id", -1);
                r.zoneId = jr.value("zone_id", -1);
                r.localX = jr.value("local_x", 0.0f);
                r.localZ = jr.value("local_z", 0.0f);
                r.category = static_cast<ReportCategory>(jr.value("category", static_cast<int>(ReportCategory::Other)));
                r.status = static_cast<ReportStatus>(jr.value("status", static_cast<int>(ReportStatus::New)));
                r.timestamp = jr.value("timestamp", std::string());
                r.description = jr.value("description", std::string());
                m_reports.push_back(r);
            }
            LogInfo("DigitalTwin::LoadReports: loaded " + std::to_string(m_reports.size()) + " reports from " + path);
        } catch (const std::exception& e) {
            LogWarn(std::string("DigitalTwin::LoadReports: failed to parse ") + path + ": " + e.what());
        }
    }

    const Zone* DigitalTwin::FindZone(int zoneId) const {
        auto it = std::find_if(m_zones.begin(), m_zones.end(),
            [zoneId](const Zone& z) { return z.id == zoneId; });
        return it != m_zones.end() ? &(*it) : nullptr;
    }

}  // namespace twin