#include "Inspector.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <iamgui/imgui.h>
#include "../twin/HeatLayers.h"

namespace twin {

    namespace {
        // Small helper so every "still placeholder" field renders the same
        // way instead of each call site inventing its own dash/label.
        void LabeledValue(const char* label, const std::string& value, bool isPlaceholder) {
            ImGui::TextUnformatted(label);
            ImGui::SameLine(160.0f);
            if (isPlaceholder) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s  (placeholder)", value.c_str());
            } else {
                ImGui::TextUnformatted(value.c_str());
            }
        }
    }

    void Inspector::Render(const Zone* zone) {
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Zone Inspector")) {
            ImGui::End();
            return;
        }

        if (!zone) {
            ImGui::TextWrapped("Click a zone in the 3D view to inspect it.");
            ImGui::End();
            return;
        }

        RenderZoneDetails(*zone);
        ImGui::End();
    }

    void Inspector::RenderZoneDetails(const Zone& zone) {
        ImGui::Text("ZONE %d", zone.id);
        ImGui::Separator();

        char buf[64];

        std::snprintf(buf, sizeof(buf), "%.1f C", zone.temperature);
        LabeledValue("Temperature", buf, zone.temperatureIsPlaceholder);

        std::snprintf(buf, sizeof(buf), "%d", zone.population);
        LabeledValue("Population", buf, zone.populationIsPlaceholder);

        if (!zone.populationIsPlaceholder) {
            std::snprintf(buf, sizeof(buf), "%.0f / km^2", zone.populationDensity);
            LabeledValue("Pop. Density", buf, false);
        }

        // Air quality (Phase 26)
        if (zone.airQualityIsPlaceholder) {
            LabeledValue("PM2.5", std::string("--"), true);
            LabeledValue("AQI", std::string("--"), true);
        } else {
            std::snprintf(buf, sizeof(buf), "%.1f µg/m3", zone.pm25);
            LabeledValue("PM2.5", buf, false);
            std::snprintf(buf, sizeof(buf), "%.0f", zone.aqi);
            LabeledValue("AQI", buf, false);
        }

        std::snprintf(buf, sizeof(buf), "%.1f%%", zone.greenCoverage * 100.0f);
        LabeledValue("Green Coverage", buf, false);  // real since Build(), Phase 3

        std::snprintf(buf, sizeof(buf), "%.1f%%", zone.buildingDensity * 100.0f);
        LabeledValue("Building Density", buf, false);  // real since Build(), Phase 3

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextUnformatted("HEAT RISK");
        if (zone.riskIsPlaceholder) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                "-- / 100  (needs temperature + population)");
        } else {
            ImGui::Text("%.0f / 100  (%s)", zone.heatRisk, zone.riskClass.c_str());
            if (!zone.populationExposureIsPlaceholder) {
                ImGui::Text("Exposed population: %d", zone.heatExposedPopulation);
                ImGui::TextDisabled("Risk-weighted modelled estimate");
            }

            // Phase 20: What-If Intervention Comparison
            if (zone.baselineHeatRisk > 0.0f && std::fabs(zone.heatRisk - zone.baselineHeatRisk) >= 0.1f) {
                float change = zone.heatRisk - zone.baselineHeatRisk;
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.40f, 1.0f), "WHAT-IF INTERVENTION");
                ImGui::Text("BEFORE: %.0f / 100", zone.baselineHeatRisk);
                ImGui::Text("AFTER:  %.0f / 100", zone.heatRisk);
                if (change < 0.0f) {
                    ImGui::TextColored(ImVec4(0.30f, 0.85f, 0.30f, 1.0f), "CHANGE: %.0f points", change);
                } else {
                    ImGui::TextColored(ImVec4(0.90f, 0.30f, 0.30f, 1.0f), "CHANGE: +%.0f points", change);
                }
                ImGui::TextDisabled("MODELLED SCENARIO ESTIMATE");
            }
        }

        // Combined Environmental Burden (Phase 26) — a conceptual overlay
        // blending heat risk with air quality. Rendered in its own block,
        // visually separated from HEAT RISK above and PRIORITY below, and
        // never phrased as though it were part of either score — this
        // panel must keep it clearly distinguished from the core heat-risk
        // model per the master spec.
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.55f, 0.75f, 0.95f, 1.0f), "COMBINED ENVIRONMENTAL BURDEN");
        if (zone.combinedBurdenIsPlaceholder) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                "-- / 100  (needs heat risk + air quality)");
        } else {
            ImGui::Text("%.0f / 100", zone.combinedEnvironmentalBurden);
        }
        ImGui::TextDisabled("Conceptual overlay (heat + air quality).");
        ImGui::TextDisabled("Not part of the core heat-risk or priority score.");

        ImGui::Spacing();
        ImGui::TextUnformatted("PRIORITY");
        if (zone.priorityIsPlaceholder) {
            // Priority is built on top of heat risk (PriorityModel), so a
            // zone that hasn't been risk-scored yet honestly says so here
            // rather than showing a stray 0 — same rule Phase 8's
            // riskIsPlaceholder branch above already follows.
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
                                "-- / 100  (needs heat risk first)");
        } else {
            ImGui::Text("%.0f / 100   (rank #%d)", zone.priority, zone.priorityRank);

            ImGui::Spacing();
            ImGui::TextUnformatted("WHY THIS ZONE?");
            ImGui::BulletText("Heat risk: %.0f / 100 (%s)", zone.heatRisk, zone.riskClass.c_str());
            ImGui::BulletText("Green coverage: %.1f%% (low coverage raises risk)",
                               zone.greenCoverage * 100.0f);
            ImGui::BulletText("Building density: %.1f%%", zone.buildingDensity * 100.0f);
            ImGui::BulletText("Population exposure: %.0f%% (relative to this study area)",
                               zone.exposure * 100.0f);
            ImGui::BulletText("Population affected: %d people", zone.population);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Prototype decision-support score, not a validated");
        ImGui::TextDisabled("medical or scientific heat-risk measurement.");
    }

    int Inspector::RenderTopPriorityPanel(const std::vector<Zone>& zones, int maxEntries) {
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 340.0f), ImGuiCond_FirstUseEver);

        int clickedZoneId = -1;

        if (!ImGui::Begin("Top Priority Zones")) {
            ImGui::End();
            return clickedZoneId;
        }

        // Gather ranked zones. Ranking itself is DigitalTwin::ComputePriority()'s
        // job (via PriorityModel) — this panel only sorts by the rank that's
        // already been written onto each zone, it never scores anything.
        std::vector<const Zone*> ranked;
        for (const auto& z : zones) {
            if (!z.priorityIsPlaceholder) ranked.push_back(&z);
        }

        if (ranked.empty()) {
            ImGui::TextWrapped("No zones ranked yet — priority needs a real heat-risk "
                                "score first (temperature + population data).");
            ImGui::End();
            return clickedZoneId;
        }

        std::sort(ranked.begin(), ranked.end(),
                   [](const Zone* a, const Zone* b) { return a->priorityRank < b->priorityRank; });

        int shown = 0;
        for (const Zone* z : ranked) {
            if (shown >= maxEntries) break;
            ++shown;

            char label[96];
            std::snprintf(label, sizeof(label), "#%d   Zone %d   Priority %.0f",
                           z->priorityRank, z->id, z->priority);

            // A Selectable click is how this panel reports "the user picked
            // this zone" back to Application — it doesn't select the zone
            // itself (see header doc comment).
            if (ImGui::Selectable(label)) {
                clickedZoneId = z->id;
            }
            ImGui::SameLine(250.0f);
            ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "Pop %d", z->population);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Click a zone to focus the camera and inspect it.");

        ImGui::End();
        return clickedZoneId;
    }

}  // namespace twin
