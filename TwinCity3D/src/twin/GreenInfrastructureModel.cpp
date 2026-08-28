#include "GreenInfrastructureModel.h"
#include "../core/Log.h"

#include <algorithm>
#include <limits>
#include <numeric>

namespace twin {

    const char* GreenInfrastructureModel::Classify(float greenPriority0to100) {
        if (greenPriority0to100 >= 75.0f) return "Urgent";
        if (greenPriority0to100 >= 55.0f) return "High";
        if (greenPriority0to100 >= 35.0f) return "Moderate";
        return "Low";
    }

    void GreenInfrastructureModel::Compute(std::vector<Zone>& zones,
        const GreenPriorityWeights& weights,
        float targetGreenCoverage) {

        if (zones.empty()) {
            LogWarn("GreenInfrastructureModel::Compute: no zones to analyse");
            return;
        }

        const float weightSum = weights.Sum();
        if (weightSum <= 0.0f) {
            LogError("GreenInfrastructureModel::Compute: weights sum to <= 0 — "
                "refusing to compute (would divide by zero)");
            return;
        }

        // ---------------------------------------------------------------
        // Pass 1: determine the target coverage.
        //
        // All zones always have greenCoverage (geometry-derived in Build(),
        // Phase 3 — never a placeholder), so the study-area mean is always
        // computable even before weather/population/heat-risk data lands.
        // ---------------------------------------------------------------
        float sumCoverage = 0.0f;
        for (const auto& z : zones) sumCoverage += z.greenCoverage;
        const float meanCoverage = sumCoverage / static_cast<float>(zones.size());

        // If caller passed the default sentinel (<=0) use the study-area
        // mean as the target instead of the fixed kDefaultTargetGreenCoverage.
        const float effectiveTarget = (targetGreenCoverage > 0.0f)
            ? targetGreenCoverage
            : meanCoverage;

        // ---------------------------------------------------------------
        // Pass 2: per-zone greenDeficit (0..1 normalized).
        //
        // Raw deficit = max(0, target - zone.greenCoverage)
        // We then normalize across the study area so the zone with the
        // worst raw deficit gets score 1.0 and those at/above target get
        // 0.0 — this makes the deficit signal comparable to the 0..1
        // heat-risk and population-exposure signals in Pass 3.
        // ---------------------------------------------------------------
        float maxRawDeficit = 0.0f;
        std::vector<float> rawDeficits(zones.size(), 0.0f);
        for (size_t i = 0; i < zones.size(); ++i) {
            rawDeficits[i] = std::max(0.0f, effectiveTarget - zones[i].greenCoverage);
            maxRawDeficit = std::max(maxRawDeficit, rawDeficits[i]);
        }

        // Avoid divide-by-zero if every zone already meets the target —
        // that means no green deficit anywhere, so every score is 0.
        const bool allAboveTarget = (maxRawDeficit <= 0.0f);

        for (size_t i = 0; i < zones.size(); ++i) {
            zones[i].greenDeficit = allAboveTarget
                ? 0.0f
                : std::clamp(rawDeficits[i] / maxRawDeficit, 0.0f, 1.0f);
        }

        // ---------------------------------------------------------------
        // Pass 3: greenPriority (0..100) and greenBenefitScore (0..1).
        //
        // Uses heatRisk and exposure if available (not placeholder),
        // falls back to 0.5 neutrals otherwise — matching the "never
        // fabricate from incomplete data" rule. greenPriority is still
        // meaningful (driven by greenDeficit alone) even if heat risk
        // isn't scored yet; this is different from PriorityModel/
        // HeatRiskModel which refuse to score at all without real inputs.
        // ---------------------------------------------------------------
        std::vector<Zone*> scoredZones;
        scoredZones.reserve(zones.size());

        for (auto& zone : zones) {
            float deficitScore = zone.greenDeficit;   // already 0..1

            float heatScore = zone.riskIsPlaceholder
                ? 0.5f  // neutral fallback when heat risk not yet computed
                : std::clamp(zone.heatRisk / 100.0f, 0.0f, 1.0f);

            float exposureScore = zone.riskIsPlaceholder
                ? 0.5f
                : std::clamp(zone.exposure, 0.0f, 1.0f);

            float weighted =
                weights.greenDeficit       * deficitScore +
                weights.heatRisk           * heatScore +
                weights.populationExposure * exposureScore;

            zone.greenPriority = std::clamp((weighted / weightSum) * 100.0f, 0.0f, 100.0f);
            zone.greenInfraIsPlaceholder = false;
            scoredZones.push_back(&zone);

            // greenBenefitScore: fraction of current heat risk attributable
            // to green deficit × the deficit fraction.
            //   = deficitScore * greenDeficit_weight_in_heat_model * heatRisk
            // Using HeatRiskWeights default greenDeficit=0.20 as a constant
            // because the model's HeatRiskWeights are not in scope here —
            // keeps GreenInfrastructureModel self-contained and this
            // derivation is clearly labelled MODELLED SCENARIO ESTIMATE.
            constexpr float kHeatModelGreenDeficitWeight = 0.20f;
            if (!zone.riskIsPlaceholder) {
                zone.greenBenefitScore = std::clamp(
                    deficitScore * kHeatModelGreenDeficitWeight * (zone.heatRisk / 100.0f),
                    0.0f, 1.0f);
            } else {
                zone.greenBenefitScore = 0.0f;
            }
        }

        // ---------------------------------------------------------------
        // Pass 4: assign greenPriorityRank (1 = most urgent).
        // ---------------------------------------------------------------
        std::sort(scoredZones.begin(), scoredZones.end(),
            [](const Zone* a, const Zone* b) { return a->greenPriority > b->greenPriority; });
        for (size_t i = 0; i < scoredZones.size(); ++i) {
            scoredZones[i]->greenPriorityRank = static_cast<int>(i) + 1;
        }

        LogInfo("GreenInfrastructureModel::Compute: analysed " +
            std::to_string(zones.size()) + " zones — mean coverage=" +
            std::to_string(meanCoverage * 100.0f) + "% target=" +
            std::to_string(effectiveTarget * 100.0f) + "% "
            "max deficit=" + std::to_string(maxRawDeficit * 100.0f) + "% "
            "— PROTOTYPE DECISION-SUPPORT INDICATOR, not a validated green-space standard");
    }

}  // namespace twin
