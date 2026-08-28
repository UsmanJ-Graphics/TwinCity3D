#pragma once
#include "Zone.h"
#include <vector>

namespace twin {

    // Phase 19. Configurable weights for the green-priority score.
    //
    // GreenPriority answers "WHERE should we add vegetation first" —
    // deliberately NOT the same question as HeatRiskModel's "how hot is
    // this zone". A cool, sparsely populated zone with no greenery should
    // NOT outrank a hot, densely populated one just because its green
    // deficit is larger. This model ensures all three signals combine:
    //
    //   GreenPriority = 0.40*GreenDeficitScore
    //                 + 0.35*HeatRiskScore
    //                 + 0.25*PopulationExposureScore
    //
    // Kept as a struct (same reasoning as HeatRiskWeights / PriorityWeights)
    // so the ScenarioEngine/UI can retune without touching this class.
    struct GreenPriorityWeights {
        float greenDeficit       = 0.40f;  // normalized deficit below study-area mean
        float heatRisk           = 0.35f;  // zone.heatRisk / 100
        float populationExposure = 0.25f;  // zone.exposure (relative, Phase 8)

        float Sum() const {
            return greenDeficit + heatRisk + populationExposure;
        }
    };

    // Phase 19. PROTOTYPE GREEN INFRASTRUCTURE DECISION-SUPPORT LAYER.
    //
    // This is NOT a validated ecological or urban-planning model. It
    // ranks zones relative to each other within this one study area to
    // support planner decision-making. Scores should always be displayed
    // with the "Prototype modelled scenario estimate" label in the UI.
    //
    // What this computes:
    //   greenDeficit        — how far below the study-area mean green
    //                         coverage this zone is, normalized 0..1.
    //                         Zones at or above the mean get 0.
    //   greenPriority       — composite WHERE-to-add-greenery score 0..100.
    //   greenPriorityRank   — 1 = greatest green infrastructure need.
    //   greenBenefitScore   — 0..1 prototype estimate of heat-risk
    //                         reduction potential if green coverage were
    //                         raised to the study-area target. Derived as
    //                         greenDeficit * HeatRiskModel::kGreenDeficitWeight
    //                         (i.e. the fraction of heat risk attributable to
    //                         the green deficit for that zone) — never claimed
    //                         to be a measured or validated figure.
    //
    // Safe to call after heatRisk has been computed (riskIsPlaceholder ==
    // false). Zones where riskIsPlaceholder is still true are scored on
    // greenDeficit alone so greenPriority is still meaningful even without
    // complete heat-risk data, but greenBenefitScore is only computed when
    // heat risk is real.
    class GreenInfrastructureModel {
    public:
        // targetGreenCoverage: fraction (0..1) to use as "what this study
        // area should aim for." 0.20 is a reasonable urban-greening target
        // for dense South Asian cities; callers may override. If <=0, the
        // model uses the study-area mean as the target instead — an easier
        // ask than a fixed absolute target, which is still useful for
        // identifying the most deficit-stricken zones.
        static constexpr float kDefaultTargetGreenCoverage = 0.20f;

        static void Compute(std::vector<Zone>& zones,
            const GreenPriorityWeights& weights = GreenPriorityWeights{},
            float targetGreenCoverage = kDefaultTargetGreenCoverage);

        // Returns a short textual category for the green-priority score.
        // "Urgent"       >= 75
        // "High"         >= 55
        // "Moderate"     >= 35
        // "Low"          >= 0
        static const char* Classify(float greenPriority0to100);
    };

}  // namespace twin
