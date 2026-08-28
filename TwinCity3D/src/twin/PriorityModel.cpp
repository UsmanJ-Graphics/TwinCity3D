#include "PriorityModel.h"
#include "../core/Log.h"

#include <algorithm>
#include <limits>

namespace twin {

    void PriorityModel::Compute(std::vector<Zone>& zones, const PriorityWeights& weights) {
        if (zones.empty()) {
            LogWarn("PriorityModel::Compute: no zones to rank");
            return;
        }

        const float weightSum = weights.Sum();
        if (weightSum <= 0.0f) {
            LogError("PriorityModel::Compute: weights sum to <= 0 — refusing to compute "
                "(would divide by zero); leaving all zones as placeholders");
            return;
        }

        // Pass 1: population-magnitude (raw headcount) min/max, across
        // zones that both have a real heat-risk score AND real population —
        // a zone missing either can't be ranked at all (see Compute()'s
        // doc comment), so it's excluded here too rather than skewing the
        // normalization for zones that will actually be scored.
        float minPop = std::numeric_limits<float>::max();
        float maxPop = std::numeric_limits<float>::lowest();
        int countable = 0;
        for (const auto& z : zones) {
            if (z.riskIsPlaceholder || z.populationIsPlaceholder) continue;
            minPop = std::min(minPop, static_cast<float>(z.population));
            maxPop = std::max(maxPop, static_cast<float>(z.population));
            ++countable;
        }

        const float popRange = maxPop - minPop;
        // Same fallback reasoning as HeatRiskModel::Compute()'s
        // useFlatExposure: fewer than 2 comparable zones, or identical
        // population, makes min-max normalization undefined.
        const bool useFlatPopMagnitude = (countable < 2) || (popRange <= 0.0f);

        // Pass 2: score + rank. Zones without a real heat-risk score are
        // left untouched (priorityIsPlaceholder stays true, priorityRank
        // stays unassigned) — priority can never be more complete than the
        // heat-risk score it's built on.
        std::vector<Zone*> scoredZones;
        int skipped = 0;

        for (auto& zone : zones) {
            if (zone.riskIsPlaceholder) {
                zone.priorityIsPlaceholder = true;
                ++skipped;
                continue;
            }

            float heatRiskScore = std::clamp(zone.heatRisk / 100.0f, 0.0f, 1.0f);

            float populationMagnitudeScore = useFlatPopMagnitude
                ? 0.5f
                : std::clamp((static_cast<float>(zone.population) - minPop) / popRange, 0.0f, 1.0f);

            float environmentalBurdenScore = std::clamp(zone.environmentalHeatBurden, 0.0f, 1.0f);
            float populationExposureScore = std::clamp(zone.exposure, 0.0f, 1.0f);

            float weightedSum =
                weights.heatRisk * heatRiskScore +
                weights.populationMagnitude * populationMagnitudeScore +
                weights.environmentalBurden * environmentalBurdenScore +
                weights.populationExposure * populationExposureScore;

            zone.priority = std::clamp((weightedSum / weightSum) * 100.0f, 0.0f, 100.0f);
            zone.priorityIsPlaceholder = false;
            scoredZones.push_back(&zone);
        }

        // Rank among scored zones only: 1 = highest priority. Sorting
        // pointers into m_zones rather than copying, so this writes rank
        // directly back onto the real Zone objects.
        std::sort(scoredZones.begin(), scoredZones.end(),
            [](const Zone* a, const Zone* b) { return a->priority > b->priority; });
        for (size_t i = 0; i < scoredZones.size(); ++i) {
            scoredZones[i]->priorityRank = static_cast<int>(i) + 1;
        }

        if (skipped > 0) {
            LogWarn("PriorityModel::Compute: skipped " + std::to_string(skipped) +
                " zone(s) without a real heat-risk score — priority stays placeholder there");
        }

        if (useFlatPopMagnitude) {
            LogWarn("PriorityModel::Compute: population had no usable spread across scoreable "
                "zones (< 2 zones, or all identical) — populationMagnitudeScore fell back to a "
                "flat 0.5 for every scored zone");
        }

        LogInfo("PriorityModel::Compute: ranked " + std::to_string(scoredZones.size()) + "/" +
            std::to_string(zones.size()) + " zones — PROTOTYPE DECISION-SUPPORT PRIORITY, not a "
            "validated government prioritization methodology, weights=(heatRisk=" +
            std::to_string(weights.heatRisk) + " popMagnitude=" +
            std::to_string(weights.populationMagnitude) + " envBurden=" +
            std::to_string(weights.environmentalBurden) + " popExposure=" +
            std::to_string(weights.populationExposure) + ")");
    }

}  // namespace twin
