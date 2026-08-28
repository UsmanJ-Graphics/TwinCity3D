#pragma once
#include "Zone.h"
#include "WeatherData.h"
#include "PopulationData.h"
#include "SatelliteEnvironmentData.h"
#include "HeatRiskModel.h"
#include "PriorityModel.h"
#include "../gis/GISTypes.h"
#include <vector>

namespace twin {

    // Owns the digital twin's zone grid and keeps each Zone's data-model fields
    // in sync with whatever real data is available. Phase 4: builds zones
    // from gis::ZoneBounds and fills in buildingDensity/greenCoverage from
    // Phase 3 geometry. Phase 5 adds ApplyWeather(), which clears the
    // temperature placeholder. Phase 6 adds the environmental burden layer
    // (exposedSurfaceRatio/environmentalHeatBurden) and uses it to spread the
    // single area-wide weather reading across zones instead of giving every
    // zone an identical value. Phase 7 adds ApplyPopulation(), which clears
    // the population placeholder from a modelled/WorldPop-derived estimate.
    // Phase 8 adds ComputeHeatRisk(), which combines all of the above into
    // an explainable heat-risk score per zone. Phase 11 adds
    // ComputePriority(), which ranks zones for government intervention on
    // top of that heat-risk score — the zone list itself doesn't change
    // shape again.
    class DigitalTwin {
    public:
        // Builds one Zone per gis::ZoneBounds entry, computes the
        // geometry-derived fields (density, green coverage), and then the
        // Phase 6 environmental burden layer derived from those.
        void Build(const gis::GISDataset& dataset);

        // Phase 5/6: applies current weather to every zone. There's still no
        // real per-zone spatial temperature model (that would need a proper
        // microclimate simulation, out of MVP scope) — instead, Phase 6 nudges
        // each zone's temperature away from the single station reading using
        // that zone's environmentalHeatBurden relative to the study-area
        // average, so denser/greener-deficit zones read a bit warmer and
        // greener zones a bit cooler. This is a MODELLED OFFSET for
        // visualization purposes, not a second measurement — always logged as
        // such, never presented as validated data (Critical Engineering Rule
        // #2/#3). Does nothing (leaves temperatureIsPlaceholder true on every
        // zone) if weather.valid is false, so a missing/broken weather.json
        // can never masquerade as a real reading.
        void ApplyWeather(const WeatherData& weather);

        // Phase 15: applies a uniform, explicitly modelled heatwave offset
        // after ApplyWeather() has restored the current-condition baseline.
        // This never changes the cached weather observation; it only changes
        // the in-memory scenario state used by the renderer and score models.
        void ApplyTemperatureOffset(float deltaC);

        // Phase 16: lightweight rainfall/surface-susceptibility flood-risk
        // visualization; intentionally not hydrological/elevation modelling.
        void ComputeFloodRisk(float rainfallMm);

        // Phase 7: applies estimated/WorldPop-derived population to every
        // zone that has a matching entry in `population`. Matches by zone
        // id via FindZone() rather than assuming index alignment, since
        // population.json's zone list and this class's m_zones aren't
        // guaranteed to be in the same order or even the same length. Does
        // nothing (leaves populationIsPlaceholder true on every zone) if
        // population.valid is false, mirroring ApplyWeather()'s contract —
        // a missing/broken population.json can never masquerade as real data.
        void ApplyPopulation(const PopulationData& population);
        void ApplySatelliteEnvironment(const SatelliteEnvironmentData& environment);

        // Phase 8: computes heatRisk/exposure/riskClass on every zone from
        // fields already present on it (temperature from Phase 5/6,
        // buildingDensity/greenCoverage from Phase 3, populationDensity from
        // Phase 7) — no new external data source, purely a combination step.
        // Delegates the actual scoring to HeatRiskModel so the algorithm
        // stays unit-testable and its weights stay configurable independent
        // of this class. A zone whose temperature or population is still a
        // placeholder is left with riskIsPlaceholder == true — this can
        // never fabricate a score from incomplete inputs. Safe to call again
        // (e.g. after Phase 12's heatwave scenario changes zone.temperature)
        // to recompute risk under a new scenario.
        void ComputeHeatRisk(const HeatRiskWeights& weights = HeatRiskWeights{});

        // Phase 17: converts risk and resident population into transparent,
        // heat-risk-weighted population-exposure indicators after risk scoring.
        void ComputePopulationExposure();

        // Phase 11: computes priority (0..100) and priorityRank on every
        // zone that already has a real heat-risk score. This does NOT
        // recompute heat risk — it's a further explainable combination of
        // heatRisk, environmentalHeatBurden, and population (both raw
        // magnitude and density-relative exposure), delegated to
        // PriorityModel so the ranking algorithm stays independently
        // tunable/testable, mirroring how ComputeHeatRisk() delegates to
        // HeatRiskModel. A zone whose heat risk is still a placeholder is
        // left with priorityIsPlaceholder == true and no rank — priority
        // can never be more complete than the score it's built on. Safe to
        // call again after a later scenario (Phase 12+) recomputes heat
        // risk under new conditions, to re-rank zones accordingly.
        void ComputePriority(const PriorityWeights& weights = PriorityWeights{});

        const std::vector<Zone>& Zones() const { return m_zones; }
        std::vector<Zone>& Zones() { return m_zones; }

        // Returns nullptr if no zone with that id exists.
        Zone* FindZone(int zoneId);
        const Zone* FindZone(int zoneId) const;

    private:
        // Phase 6: fills exposedSurfaceRatio and environmentalHeatBurden on
        // every zone from the buildingDensity/greenCoverage already computed
        // in Build(). Pure arithmetic over existing fields — no new data
        // source, no per-frame cost, matches Phase 19's "no expensive work in
        // the render loop" rule trivially since this only runs once at load.
        void ComputeEnvironmentalLayer();

        std::vector<Zone> m_zones;
    };

}  // namespace twin
