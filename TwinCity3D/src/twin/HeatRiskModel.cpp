#include "HeatRiskModel.h"
#include "../core/Log.h"

#include <algorithm>
#include <limits>

namespace twin {

    std::string HeatRiskModel::Classify(float heatRisk0to100) {
        if (heatRisk0to100 <= 30.0f) return "Low";
        if (heatRisk0to100 <= 50.0f) return "Moderate";
        if (heatRisk0to100 <= 70.0f) return "High";
        if (heatRisk0to100 <= 85.0f) return "Very High";
        return "Extreme";
    }

    void HeatRiskModel::Compute(std::vector<Zone>& zones, const HeatRiskWeights& weights) {
        if (zones.empty()) {
            LogWarn("HeatRiskModel::Compute: no zones to score");
            return;
        }

        const float weightSum = weights.Sum();
        if (weightSum <= 0.0f) {
            LogError("HeatRiskModel::Compute: weights sum to <= 0 — refusing to compute "
                "(would divide by zero); leaving all zones as placeholders");
            return;
        }

        // Pass 1: population-exposure min/max, across zones with real
        // (non-placeholder) population only. A zone with placeholder
        // population is excluded from the min/max AND will be skipped
        // entirely in pass 2, so it can't skew the normalization for zones
        // that do have real data.
        float minDensity = std::numeric_limits<float>::max();
        float maxDensity = std::numeric_limits<float>::lowest();
        int zonesWithPopulation = 0;
        for (const auto& z : zones) {
            if (z.populationIsPlaceholder) continue;
            minDensity = std::min(minDensity, z.populationDensity);
            maxDensity = std::max(maxDensity, z.populationDensity);
            ++zonesWithPopulation;
        }

        const float densityRange = maxDensity - minDensity;
        // Fires when every zone with real population has identical density,
        // or when only one such zone exists — min-max normalization is
        // undefined there, so every zone falls back to a neutral 0.5 rather
        // than an arbitrary 0.0 or 1.0.
        const bool useFlatExposure = (zonesWithPopulation < 2) || (densityRange <= 0.0f);

        // Pass 2: compute the four component scores and the weighted risk
        // per zone.
        int scored = 0, skippedTemperature = 0, skippedPopulation = 0;
        float minRisk = 0.0f, maxRisk = 0.0f;

        for (auto& zone : zones) {
            if (zone.temperatureIsPlaceholder) { ++skippedTemperature; continue; }
            if (zone.populationIsPlaceholder) { ++skippedPopulation; continue; }

            float temperatureScore = std::clamp(
                (zone.temperature - kTempScoreMinC) / (kTempScoreMaxC - kTempScoreMinC),
                0.0f, 1.0f);

            float greenDeficitScore = std::clamp(1.0f - zone.greenCoverage, 0.0f, 1.0f);
            float buildingDensityScore = std::clamp(zone.buildingDensity, 0.0f, 1.0f);

            float populationExposureScore = useFlatExposure
                ? 0.5f
                : std::clamp((zone.populationDensity - minDensity) / densityRange, 0.0f, 1.0f);

            float weightedSum =
                weights.temperature * temperatureScore +
                weights.greenDeficit * greenDeficitScore +
                weights.buildingDensity * buildingDensityScore +
                weights.populationExposure * populationExposureScore;

            float heatRisk = std::clamp((weightedSum / weightSum) * 100.0f, 0.0f, 100.0f);

            zone.exposure = populationExposureScore;
            zone.heatRisk = heatRisk;
            zone.riskClass = Classify(heatRisk);
            zone.riskIsPlaceholder = false;

            if (scored == 0) { minRisk = maxRisk = heatRisk; }
            else {
                minRisk = std::min(minRisk, heatRisk);
                maxRisk = std::max(maxRisk, heatRisk);
            }
            ++scored;
        }

        if (skippedTemperature > 0 || skippedPopulation > 0) {
            LogWarn("HeatRiskModel::Compute: skipped " + std::to_string(skippedTemperature) +
                " zone(s) with placeholder temperature and " + std::to_string(skippedPopulation) +
                " zone(s) with placeholder population — those zones keep riskIsPlaceholder=true");
        }

        if (useFlatExposure) {
            LogWarn("HeatRiskModel::Compute: population density had no usable spread across "
                "zones (< 2 zones with real population, or all identical) — populationExposureScore "
                "fell back to a flat 0.5 for every scored zone");
        }

        LogInfo("HeatRiskModel::Compute: scored " + std::to_string(scored) + "/" +
            std::to_string(zones.size()) + " zones — heatRisk range [" +
            std::to_string(minRisk) + ", " + std::to_string(maxRisk) +
            "] — PROTOTYPE DECISION-SUPPORT SCORE, not a validated medical/scientific model, weights=" +
            "(temp=" + std::to_string(weights.temperature) +
            " greenDeficit=" + std::to_string(weights.greenDeficit) +
            " buildingDensity=" + std::to_string(weights.buildingDensity) +
            " populationExposure=" + std::to_string(weights.populationExposure) + ")");
    }

}  // namespace twin
