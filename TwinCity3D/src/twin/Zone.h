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
    //   buildingDensity, greenCoverage           -> computed now, from Phase 3 geometry
    //   exposedSurfaceRatio, environmentalHeatBurden -> computed now, Phase 6 (derived, not placeholders)
    //   temperature                              -> computed now, Phase 5/6 (modelled spread of one weather reading)
    //   population, populationDensity            -> computed now, Phase 7 (estimated model or
    //                                                WorldPop-total-rescaled; see population.json)
    //   heatRisk, exposure, riskClass             -> computed now, Phase 8 (explainable prototype score;
    //                                                see HeatRiskModel — requires real temperature AND
    //                                                real population on the zone, else stays placeholder)
    //   priority                                  -> placeholder until Phase 11
    struct Zone {
        int id{ -1 };

        // Local-space bounds (meters), from Phase 2's zone grid.
        float minX{ 0.0f }, maxX{ 0.0f }, minZ{ 0.0f }, maxZ{ 0.0f };

        // --- Population ---
        int population{ 0 };              // placeholder = 0 until Phase 7 (WorldPop aggregation)
        float populationDensity{ 0.0f };  // persons per km^2; placeholder = 0 until Phase 7
        bool populationIsPlaceholder{ true };

        // --- Environment (0..1 fractions unless noted) ---
        float greenCoverage{ 0.0f };      // real: green-area footprint area / zone area
        float buildingDensity{ 0.0f };    // real: building footprint area / zone area

        // Phase 6: cheap proxy for "everything that's neither a building roof
        // nor vegetation" (bare ground, pavement, parking, unclassified OSM
        // gaps) — 1 - buildingDensity - greenCoverage, clamped. Not a real
        // land-cover classification (that would need satellite/Python
        // preprocessing, which the master spec explicitly says to avoid doing
        // in C++); it's a bookkeeping remainder used only to weight heat burden.
        float exposedSurfaceRatio{ 0.0f };

        // Phase 6: 0..1 composite of buildingDensity, green deficit (1 -
        // greenCoverage), and exposedSurfaceRatio. This is what lets Phase 6's
        // ApplyWeather() spread the single area-wide weather reading across
        // zones instead of giving every zone an identical number — a hot,
        // paved, green-poor zone reads a bit warmer than a leafy one even
        // though both use the same station data. Purely a relative weighting
        // signal, not a measured temperature; feeds into Phase 8's
        // GreenDeficitScore/BuildingDensityScore too.
        float environmentalHeatBurden{ 0.0f };

        float temperature{ 0.0f };        // degrees C; placeholder until Phase 5/6
        bool temperatureIsPlaceholder{ true };

        // --- Derived risk (Phase 8/11) ---
        float heatRisk{ 0.0f };           // 0..100, explainable prototype score — see HeatRiskModel
        float exposure{ 0.0f };           // 0..1, Phase 8's PopulationExposureScore (relative to this
                                           // study area's own population-density spread, not an absolute scale)
        std::string riskClass;            // Phase 8 classification: "Low" | "Moderate" | "High" |
                                           // "Very High" | "Extreme" (see HeatRiskModel::Classify).
                                           // Empty until riskIsPlaceholder is cleared.
        float priority{ 0.0f };           // 0..100; real since Phase 11 (see PriorityModel)
        bool riskIsPlaceholder{ true };   // covers heatRisk/exposure/riskClass only — priority has
                                           // its own flag below, since it can be computed/cleared on
                                           // a different schedule (built on top of heat risk)

        // --- Baseline Tracking (Phase 20 What-If Intervention) ---
        float baselineHeatRisk{ 0.0f };      // stores initial heatRisk before scenario / intervention
        float baselinePriority{ 0.0f };      // stores initial priority before scenario / intervention
        float baselineGreenCoverage{ 0.0f }; // stores geometry-derived green coverage baseline

        // --- Priority (Phase 11) ---
        bool priorityIsPlaceholder{ true };  // false once PriorityModel scores this zone; requires
                                              // riskIsPlaceholder == false first (priority is built
                                              // on top of heat risk, never fabricated independently)
        int priorityRank{ -1 };               // 1 = highest priority among scored zones; -1 = unranked

        // --- Population exposure (Phase 17) ---
        // Heat-risk-weighted resident counts are modelled decision-support
        // indicators, not observed incident or evacuation counts.
        int heatExposedPopulation{ 0 };
        int highRiskPopulation{ 0 };
        int extremeRiskPopulation{ 0 };
        float heatExposureScore{ 0.0f };       // relative 0..1 within study area
        bool populationExposureIsPlaceholder{ true };

        // Phase 18: either raster-preprocessed environmental data or an
        // explicitly labelled offline OSM land-cover proxy.
        float vegetationIndex{ 0.0f };
        float builtUpIndex{ 0.0f };
        bool satelliteEnvironmentIsPlaceholder{ true };

        // Phase 16: geometry/rainfall-derived prototype visualization only;
        // never a hydrological model or a flood forecast.
        float floodRisk{ 0.0f };              // 0..100
        std::string floodRiskClass;           // Low | Moderate | High | Critical
        bool floodRiskIsPlaceholder{ true };

        // Phase 19: green infrastructure analysis — WHERE to add greenery.
        //
        // greenDeficit: how far below the study-area mean green coverage this
        //   zone is, normalized 0..1 (0 = at or above the mean; 1 = maximum
        //   deficit). Never an absolute hectare target — only meaningful
        //   relative to other zones in this study area.
        // greenPriority: composite 0..100 score that answers WHERE additional
        //   vegetation would provide the greatest benefit. Combines green
        //   deficit, heat risk, and population exposure so a cool, low-
        //   population zone with little greenery doesn't outrank a hot,
        //   densely populated one. See GreenInfrastructureModel.
        // greenPriorityRank: 1 = greatest green-infrastructure need; -1 = not
        //   yet ranked.
        // greenBenefitScore: 0..1 prototype estimate of heat-risk reduction
        //   potential if vegetation coverage were brought to the study-area
        //   target. Purely modelled — see GreenInfrastructureModel for the
        //   derivation and its explicit "MODELLED SCENARIO ESTIMATE" caveat.
        //
        // All Phase 19 fields are prototype decision-support indicators.
        // Never present greenPriority as a validated ecological score.
        float greenDeficit{ 0.0f };
        float greenPriority{ 0.0f };          // 0..100
        int   greenPriorityRank{ -1 };        // 1 = highest green need
        float greenBenefitScore{ 0.0f };      // 0..1 prototype heat-reduction potential
        bool  greenInfraIsPlaceholder{ true };

        float Width() const { return maxX - minX; }
        float Depth() const { return maxZ - minZ; }
        float Area() const { return Width() * Depth(); }
    };

}  // namespace twin
