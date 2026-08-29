#include "HeatLayers.h"
#include "HeatRiskModel.h"

#include <algorithm>

namespace twin {

    DataLayer NextDataLayer(DataLayer current) {
        switch (current) {
            case DataLayer::HeatRisk:        return DataLayer::Population;
            case DataLayer::Population:      return DataLayer::PopulationExposure;
            case DataLayer::PopulationExposure:return DataLayer::GreenCoverage;
            case DataLayer::GreenCoverage:   return DataLayer::BuildingDensity;
            case DataLayer::BuildingDensity: return DataLayer::Temperature;
            case DataLayer::Temperature:     return DataLayer::FloodRisk;
            case DataLayer::FloodRisk:       return DataLayer::SatelliteEnvironment;
            case DataLayer::SatelliteEnvironment: return DataLayer::GreenPriority;
            case DataLayer::GreenPriority:   return DataLayer::AirQuality;
            case DataLayer::AirQuality:      return DataLayer::LightPollution;
            case DataLayer::LightPollution:  return DataLayer::BirdEcologicalImpact;
            case DataLayer::BirdEcologicalImpact: return DataLayer::Reports;
            case DataLayer::Reports: return DataLayer::HeatRisk;
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
            case DataLayer::PopulationExposure:
                return { "Population Exposure", "risk-weighted residents, relative", "Lower exposure", "Higher exposure" };
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
            case DataLayer::SatelliteEnvironment:
                return { "Environment", "vegetation proxy (OSM, not satellite)", "Low vegetation", "High vegetation" };
            case DataLayer::AirQuality:
                return { "Air Quality", "PM2.5 µg/m3 (normalized)", "Low PM2.5", "High PM2.5" };
            case DataLayer::GreenPriority:
                return { "Green Infrastructure Priority",
                         "WHERE to add greenery (0-100, prototype)",
                         "Low need", "Urgent need" };
            case DataLayer::LightPollution:
                return { "Light Pollution", "satellite night light index 0..1", "Dark / Low Light", "High Light Intensity" };
            case DataLayer::BirdEcologicalImpact:
                return { "Bird & Ecological Disturbance", "habitat disturbance score 0-100", "Low Disturbance", "Critical Impact" };
            case DataLayer::Reports:
                return { "Reports", "citizen reports overlay", "Hidden", "Visible" };
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
                if (range <= 0.0f) return 0.5f;
                return std::clamp((zone.populationDensity - minPopDensity) / range, 0.0f, 1.0f);
            }

            case DataLayer::PopulationExposure:
                if (zone.populationExposureIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.heatExposureScore, 0.0f, 1.0f);

            case DataLayer::GreenCoverage:
                return std::clamp(zone.greenCoverage, 0.0f, 1.0f);

            case DataLayer::BuildingDensity:
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

            case DataLayer::SatelliteEnvironment:
                if (zone.satelliteIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.vegetationIndex, 0.0f, 1.0f);

            case DataLayer::AirQuality:
                if (zone.airQualityIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.airQualityIndex, 0.0f, 1.0f);

            case DataLayer::GreenPriority:
                if (zone.greenInfraIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.greenPriority / 100.0f, 0.0f, 1.0f);

            case DataLayer::LightPollution:
                if (zone.ecologicalIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.lightPollutionIndex, 0.0f, 1.0f);

            case DataLayer::BirdEcologicalImpact:
                if (zone.ecologicalIsPlaceholder) return kNoDataSentinel;
                return std::clamp(zone.birdEcologicalDisturbance / 100.0f, 0.0f, 1.0f);
        }
        return kNoDataSentinel;
    }

}  // namespace twin
// Definitions must live in the twin namespace to match declarations.
namespace twin {
    const char* ClassifyLightPollution(float lightIndex01) {
        if (lightIndex01 < 0.0f) return "No data";
        if (lightIndex01 < 0.25f) return "Low";
        if (lightIndex01 < 0.50f) return "Moderate";
        if (lightIndex01 < 0.75f) return "High";
        return "Critical";
    }

    const char* ClassifyBirdImpact(float disturbance0to100) {
        if (disturbance0to100 < 0.0f) return "No data";
        if (disturbance0to100 < 25.0f) return "Low";
        if (disturbance0to100 < 50.0f) return "Moderate";
        if (disturbance0to100 < 75.0f) return "High";
        return "Critical";
    }
} // namespace twin
