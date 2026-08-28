#pragma once
#include "Zone.h"
#include <string>
#include <vector>

namespace twin {

    // Phase 8. Configurable weights for the explainable heat-risk score.
    // Defaults match the master spec exactly:
    //   HeatRisk = 0.35*TemperatureScore + 0.20*GreenDeficitScore
    //            + 0.20*BuildingDensityScore + 0.25*PopulationExposureScore
    //
    // Kept as a struct (not constants baked into the .cpp, as Phase 6's
    // burden weights are) specifically so the UI/ScenarioEngine can retune
    // and re-run the model later without touching this class — see Phase 8's
    // "Keep the weights configurable" requirement.
    struct HeatRiskWeights {
        float temperature = 0.35f;
        float greenDeficit = 0.20f;
        float buildingDensity = 0.20f;
        float populationExposure = 0.25f;

        // Compute() divides by this rather than assuming the four weights
        // sum to exactly 1.0, so a caller who retunes the weights (e.g. for
        // an experiment) can't silently produce an out-of-range score.
        float Sum() const {
            return temperature + greenDeficit + buildingDensity + populationExposure;
        }
    };

    // Phase 8. This is a PROTOTYPE DECISION-SUPPORT SCORE, not a validated
    // medical or scientific heat-risk model — see Critical Engineering
    // Rule #3 and the master spec's explicit disclaimer at the top of
    // Phase 8. It exists to rank zones relative to each other within this
    // one study area, not to make an absolute claim about health outcomes.
    class HeatRiskModel {
    public:
        // Temperature score normalization range (degrees C -> 0..1).
        // 25C is treated as a "low/no particular heat risk" baseline and
        // 45C as the upper end of the scale a Lahore heatwave scenario
        // (Phase 12, current temp + up to +5C delta) could plausibly reach.
        // This range is what makes TemperatureScore comparable across the
        // "current" vs "heatwave scenario" runs the ScenarioEngine will do
        // in Phase 12 — a fixed scale, not a per-run min/max, so a +4C
        // heatwave visibly raises scores instead of just re-normalizing
        // against itself.
        static constexpr float kTempScoreMinC = 25.0f;
        static constexpr float kTempScoreMaxC = 45.0f;

        // Computes heatRisk (0..100), exposure (0..1), and riskClass for
        // every zone, in place. PopulationExposureScore is normalized via
        // min/max across the zones actually passed in (not a fixed scale
        // like temperature), because population density varies by orders
        // of magnitude between study areas and there's no meaningful
        // universal ceiling the way there is for temperature — this
        // deliberately makes exposure a *relative* "who is exposed most
        // within this neighbourhood" signal, matching Phase 7's framing.
        //
        // A zone whose temperature or population is still a placeholder
        // (temperatureIsPlaceholder / populationIsPlaceholder) is skipped
        // and left with riskIsPlaceholder == true — this model never
        // fabricates a risk score from incomplete inputs. Mirrors
        // ApplyWeather()/ApplyPopulation()'s "no valid data in, no placeholder
        // cleared" contract.
        static void Compute(std::vector<Zone>& zones,
            const HeatRiskWeights& weights = HeatRiskWeights{});

        // 0-30 Low, 31-50 Moderate, 51-70 High, 71-85 Very High, 86-100 Extreme.
        static std::string Classify(float heatRisk0to100);
    };

}  // namespace twin
