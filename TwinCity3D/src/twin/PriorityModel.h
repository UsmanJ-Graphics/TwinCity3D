#pragma once
#include "Zone.h"
#include <vector>

namespace twin {

    // Phase 11. Configurable weights for the priority-ranking score.
    //
    // Priority answers "which zones should the government act on first" —
    // deliberately NOT the same question as HeatRiskModel's "how hot/exposed
    // is this zone" (see master spec: "Do not simply rank areas by
    // temperature"). heatRisk already folds in temperature, green deficit,
    // building density, and population DENSITY — Priority adds the one
    // thing heat risk deliberately leaves out: how many people, in absolute
    // terms, actually live there. That's what stops a scorching, near-empty
    // industrial zone from outranking a slightly cooler zone housing
    // thousands of residents.
    //
    //   Priority = 0.45*HeatRiskScore + 0.25*PopulationMagnitudeScore
    //            + 0.15*EnvironmentalBurdenScore + 0.15*PopulationExposureScore
    //
    // Kept as a struct (not baked-in constants), same reasoning as
    // HeatRiskWeights: a later ScenarioEngine/UI should be able to retune
    // and re-run this without touching PriorityModel itself.
    struct PriorityWeights {
        float heatRisk = 0.45f;             // zone.heatRisk / 100
        float populationMagnitude = 0.25f;  // zone.population (raw headcount), normalized across scored zones
        float environmentalBurden = 0.15f;  // zone.environmentalHeatBurden (0..1, Phase 6)
        float populationExposure = 0.15f;   // zone.exposure (0..1, population-DENSITY-relative, Phase 8)

        // Compute() divides by this rather than assuming the weights sum to
        // exactly 1.0 — same safeguard HeatRiskWeights::Sum() provides.
        float Sum() const {
            return heatRisk + populationMagnitude + environmentalBurden + populationExposure;
        }
    };

    // Phase 11. PROTOTYPE DECISION-SUPPORT PRIORITY — same disclaimer as
    // HeatRiskModel: this ranks zones relative to each other within this one
    // study area, and is not a validated government prioritization
    // methodology. Never present it as more certain than that.
    class PriorityModel {
    public:
        // Computes zone.priority (0..100) and zone.priorityRank (1 = highest
        // priority) for every zone that already has a real heat-risk score
        // (riskIsPlaceholder == false) — priority is built ON TOP OF heat
        // risk, so it can never be more "complete" than its own input. A
        // zone without a real heat-risk score keeps priorityIsPlaceholder ==
        // true and is excluded from ranking entirely (no rank assigned).
        //
        // Population-magnitude normalization is min/max across the zones
        // actually being scored here (not a fixed universal scale) — same
        // reasoning HeatRiskModel::Compute() uses for population density:
        // raw population varies wildly by study area, so there's no
        // meaningful fixed ceiling.
        static void Compute(std::vector<Zone>& zones,
            const PriorityWeights& weights = PriorityWeights{});
    };

}  // namespace twin
