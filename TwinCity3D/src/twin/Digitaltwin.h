#pragma once
#include "Zone.h"
#include "WeatherData.h"
#include "PopulationData.h"
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
    // Later phases (8, 11) call further Update*() methods on this same class
    // to fill in heatRisk, exposure, and priority — the zone list itself
    // doesn't change shape again.
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

        // Phase 7: applies estimated/WorldPop-derived population to every
        // zone that has a matching entry in `population`. Matches by zone
        // id via FindZone() rather than assuming index alignment, since
        // population.json's zone list and this class's m_zones aren't
        // guaranteed to be in the same order or even the same length. Does
        // nothing (leaves populationIsPlaceholder true on every zone) if
        // population.valid is false, mirroring ApplyWeather()'s contract —
        // a missing/broken population.json can never masquerade as real data.
        void ApplyPopulation(const PopulationData& population);

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
