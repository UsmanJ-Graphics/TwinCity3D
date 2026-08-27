#pragma once
#include <string>

namespace twin {

    // One cell of the digital twin's data model — matches Phase 4 of the
    // master spec exactly (population / greenCoverage / buildingDensity /
    // temperature / heatRisk / exposure / priority).
    //
    // This struct is deliberately just data: DigitalTwin (Step 2) is
    // responsible for filling it in from GIS + environmental + population
    // sources, and the renderer (Phase 9) is responsible for coloring geometry
    // from it. Neither rendering code nor this struct hard-codes colors or
    // risk values — see Critical Engineering Rule: "keep the heat-risk model
    // explainable" and Phase 4: "Do not hard-code visual colors or risk values
    // directly into rendering code."
    //
    // Field provenance (updated as later phases land):
    //   buildingDensity, greenCoverage  -> computed now, from Phase 3 geometry
    //   temperature                    -> placeholder until Phase 5/6
    //   population                     -> placeholder until Phase 7
    //   heatRisk, exposure, priority   -> placeholder until Phase 8/11
    struct Zone {
        int id{ -1 };

        // Local-space bounds (meters), from Phase 2's zone grid.
        float minX{ 0.0f }, maxX{ 0.0f }, minZ{ 0.0f }, maxZ{ 0.0f };

        // --- Population ---
        int population{ 0 };              // placeholder = 0 until Phase 7 (WorldPop aggregation)
        bool populationIsPlaceholder{ true };

        // --- Environment (0..1 fractions unless noted) ---
        float greenCoverage{ 0.0f };      // real: green-area footprint area / zone area
        float buildingDensity{ 0.0f };    // real: building footprint area / zone area
        float temperature{ 0.0f };        // degrees C; placeholder until Phase 5/6
        bool temperatureIsPlaceholder{ true };

        // --- Derived risk (Phase 8/11) ---
        float heatRisk{ 0.0f };           // 0..100
        float exposure{ 0.0f };           // 0..1
        float priority{ 0.0f };           // 0..100
        bool riskIsPlaceholder{ true };

        float Width() const { return maxX - minX; }
        float Depth() const { return maxZ - minZ; }
        float Area() const { return Width() * Depth(); }
    };

}  // namespace twin