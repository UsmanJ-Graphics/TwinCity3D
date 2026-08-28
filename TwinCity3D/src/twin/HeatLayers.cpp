#include "HeatLayers.h"
#include "HeatRiskModel.h"

#include <algorithm>

namespace twin {

    DataLayer NextDataLayer(DataLayer current) {
        switch (current) {
            case DataLayer::HeatRisk:        return DataLayer::Population;
            case DataLayer::Population:      return DataLayer::GreenCoverage;
            case DataLayer::GreenCoverage:   return DataLayer::BuildingDensity;
            case DataLayer::BuildingDensity: return DataLayer::Temperature;
            case DataLayer::Temperature:     return DataLayer::FloodRisk;
            case DataLayer::FloodRisk:       return DataLayer::HeatRisk;
        }
        return DataLayer::HeatRisk;
    }

    LayerInfo DescribeLayer(DataLayer layer) {
        switch (layer) {
            case DataLayer::HeatRisk:
                return { "Heat Risk", "score 0-100", "Low risk", "Extreme risk" };
            case DataLayer::Population:
                return { "Population Density", "relative, this study area only",
                         "Fewest residents", "Most residents" };
            case DataLayer::GreenCoverage:
                return { "Green Coverage", "% of zone area", "No vegetation", "Fully vegetated" };
            case DataLayer::BuildingDensity:
                return { "Building Density", "% of zone area built up", "Sparse", "Dense" };
            case DataLayer::Temperature:
                return { "Temperature",
                         std::to_string(static_cast<int>(HeatRiskModel::kTempScoreMinC)) + "-" +
                         std::to_string(static_cast<int>(HeatRiskModel::kTempScoreMaxC)) + "C scale",
                         "Cooler", "Hotter" };
            case DataLayer::FloodRisk:
                return { "Flood Risk", "prototype score 0-100", "Low", "Critical" };
        }
        return { "Unknown", "", "", "" };
    }

    float NormalizedLayerValue(const Zone& zone, DataLayer layer,
        float minPopDensity, float maxPopDensity) {
        switch (layer) {
            case DataLayer::HeatRisk:
                if (zone.riskIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.heatRisk / 100.0f, 0.0f, 1.0f);

            case DataLayer::Population: {
                if (zone.populationIsPlaceholder) return kNoDataSentinel;
                float range = maxPopDensity - minPopDensity;
                // Mirrors HeatRiskModel::Compute()'s useFlatExposure fallback:
                // no usable spread across zones (all identical, or too few
                // with real data) means min-max normalization is undefined,
                // so fall back to a neutral middle value.
                if (range <= 0.0f) return 0.5f;
                return std::clamp((zone.populationDensity - minPopDensity) / range, 0.0f, 1.0f);
            }

            case DataLayer::GreenCoverage:
                // Always geometry-derived (Phase 3), never a placeholder.
                return std::clamp(zone.greenCoverage, 0.0f, 1.0f);

            case DataLayer::BuildingDensity:
                // Always geometry-derived (Phase 3), never a placeholder.
                return std::clamp(zone.buildingDensity, 0.0f, 1.0f);

            case DataLayer::Temperature:
                if (zone.temperatureIsPlaceholder) return kNoDataSentinel;
                return std::clamp(
                    (zone.temperature - HeatRiskModel::kTempScoreMinC) /
                    (HeatRiskModel::kTempScoreMaxC - HeatRiskModel::kTempScoreMinC),
                    0.0f, 1.0f);

            case DataLayer::FloodRisk:
                if (zone.floodRiskIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.floodRisk / 100.0f, 0.0f, 1.0f);
        }
        return kNoDataSentinel;
    }

}  // namespace twin
