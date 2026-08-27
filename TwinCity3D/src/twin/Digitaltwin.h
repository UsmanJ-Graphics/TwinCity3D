#pragma once
#include "Zone.h"
#include "../gis/GISTypes.h"
#include <vector>

namespace twin {

    // Owns the digital twin's zone grid and keeps each Zone's data-model fields
    // in sync with whatever real data is available. Phase 4 only: builds zones
    // from gis::ZoneBounds and fills in buildingDensity/greenCoverage from
    // Phase 3 geometry. Later phases (5-8, 11) call further Update*() methods
    // on this same class to fill in temperature, population, heatRisk,
    // exposure, and priority — the zone list itself doesn't change shape again.
    class DigitalTwin {
    public:
        // Builds one Zone per gis::ZoneBounds entry and computes the
        // geometry-derived fields (density, green coverage) from the dataset.
        void Build(const gis::GISDataset& dataset);

        const std::vector<Zone>& Zones() const { return m_zones; }
        std::vector<Zone>& Zones() { return m_zones; }

        // Returns nullptr if no zone with that id exists.
        Zone* FindZone(int zoneId);
        const Zone* FindZone(int zoneId) const;

    private:
        std::vector<Zone> m_zones;
    };

}  // namespace twin