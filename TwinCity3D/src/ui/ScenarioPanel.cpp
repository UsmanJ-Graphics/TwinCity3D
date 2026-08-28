#include "ScenarioPanel.h"

#include <iamgui/imgui.h>
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace twin {

    namespace {
        struct ScenarioMetrics {
            int criticalZones{ 0 };
            int exposedPopulation{ 0 };
            int highRiskPopulation{ 0 };
            int extremeRiskPopulation{ 0 };
            int floodAffectedPopulation{ 0 };
            float avgHeatRiskBefore{ 0.0f };
            float avgHeatRiskAfter{ 0.0f };
            int scoredCount{ 0 };
        };

        ScenarioMetrics Measure(const std::vector<Zone>& zones) {
            ScenarioMetrics result;
            for (const Zone& zone : zones) {
                if (zone.riskIsPlaceholder || zone.populationIsPlaceholder) continue;
                if (!zone.populationExposureIsPlaceholder)
                    result.exposedPopulation += zone.heatExposedPopulation;
                if (zone.heatRisk >= 70.0f) {
                    ++result.criticalZones;
                    result.highRiskPopulation += zone.population;
                }
                if (zone.heatRisk >= 90.0f) result.extremeRiskPopulation += zone.population;
                if (!zone.floodRiskIsPlaceholder && zone.floodRisk >= 50.0f)
                    result.floodAffectedPopulation += zone.population;

                result.avgHeatRiskBefore += (zone.baselineHeatRisk > 0.0f ? zone.baselineHeatRisk : zone.heatRisk);
                result.avgHeatRiskAfter += zone.heatRisk;
                ++result.scoredCount;
            }
            if (result.scoredCount > 0) {
                result.avgHeatRiskBefore /= result.scoredCount;
                result.avgHeatRiskAfter /= result.scoredCount;
            }
            return result;
        }
    }

    bool ScenarioPanel::Render(ScenarioState& state, const std::vector<Zone>& zones,
        float baselineTemperatureC, float baselineRainfallMm, int selectedZoneId) {
        bool changed = false;
        ImGui::SetNextWindowSize(ImVec2(600.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(300.0f, 480.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Scenario Control")) {
            ImGui::End();
            return false;
        }

        ImGui::TextUnformatted("SCENARIO");
        ImGui::SameLine();

        bool isCurrent = !state.heatwaveEnabled && !state.floodEnabled && !state.interventionEnabled;
        if (ImGui::RadioButton("Current Conditions", isCurrent)) {
            state.heatwaveEnabled = false;
            state.floodEnabled = false;
            state.interventionEnabled = false;
            state.temperatureIncreaseC = 0.0f;
            state.vegetationDeltaPct = 0.0f;
            state.shadeDeltaPct = 0.0f;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Heatwave", state.heatwaveEnabled)) {
            state.heatwaveEnabled = true;
            state.floodEnabled = false;
            state.interventionEnabled = false;
            if (state.temperatureIncreaseC <= 0.0f) state.temperatureIncreaseC = 1.0f;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Flood", state.floodEnabled)) {
            state.floodEnabled = true;
            state.heatwaveEnabled = false;
            state.interventionEnabled = false;
            state.rainfallScenarioMm = 80.0f;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("What-If Intervention", state.interventionEnabled)) {
            state.interventionEnabled = true;
            state.heatwaveEnabled = false;
            state.floodEnabled = false;
            if (state.vegetationDeltaPct <= 0.0f && state.shadeDeltaPct <= 0.0f) {
                state.vegetationDeltaPct = 0.15f; // default +15%
            }
            changed = true;
        }

        // --- HEATWAVE CONTROLS ---
        if (state.heatwaveEnabled) {
            ImGui::Separator();
            ImGui::TextUnformatted("Temperature increase");
            for (int value = 1; value <= 5; ++value) {
                if (value > 1) ImGui::SameLine();
                char label[8];
                std::snprintf(label, sizeof(label), "+%dC", value);
                if (ImGui::Button(label)) {
                    state.temperatureIncreaseC = static_cast<float>(value);
                    changed = true;
                }
            }
            float previous = state.temperatureIncreaseC;
            ImGui::SliderFloat("Custom increase", &state.temperatureIncreaseC, 0.0f, 10.0f, "+%.1f C");
            if (previous != state.temperatureIncreaseC) changed = true;
        }

        // --- FLOOD CONTROLS ---
        if (state.floodEnabled) {
            ImGui::Separator();
            ImGui::TextUnformatted("Rainfall scenario");
            if (ImGui::Button("Simulate Heavy Rainfall (80 mm)")) {
                state.rainfallScenarioMm = 80.0f;
                changed = true;
            }
            float previous = state.rainfallScenarioMm;
            ImGui::SliderFloat("Rainfall", &state.rainfallScenarioMm, 0.0f, 150.0f, "%.0f mm");
            if (previous != state.rainfallScenarioMm) changed = true;
        }

        // --- WHAT-IF INTERVENTION CONTROLS (PHASE 20) ---
        if (state.interventionEnabled) {
            ImGui::Separator();
            ImGui::TextUnformatted("WHAT-IF INTERVENTION CONTROLS");
            ImGui::TextDisabled("Simulate urban greening and shade solutions");

            // Scope selection
            char scopeLabel[64];
            if (selectedZoneId >= 0) {
                std::snprintf(scopeLabel, sizeof(scopeLabel), "Apply to Selected Zone Only (Zone %d)", selectedZoneId);
            } else {
                std::snprintf(scopeLabel, sizeof(scopeLabel), "Apply to Selected Zone Only (Select a zone first)");
            }
            bool prevScope = state.applyToSelectedZoneOnly;
            if (selectedZoneId >= 0) {
                ImGui::Checkbox(scopeLabel, &state.applyToSelectedZoneOnly);
            } else {
                state.applyToSelectedZoneOnly = false;
                ImGui::TextDisabled("%s", scopeLabel);
            }
            if (prevScope != state.applyToSelectedZoneOnly) changed = true;

            ImGui::Spacing();
            ImGui::TextUnformatted("Increase Vegetation / Greenery:");
            const float vegPresets[] = { 0.05f, 0.10f, 0.15f, 0.20f };
            for (int i = 0; i < 4; ++i) {
                if (i > 0) ImGui::SameLine();
                char label[16];
                std::snprintf(label, sizeof(label), "+%d%%", static_cast<int>(vegPresets[i] * 100.0f));
                if (ImGui::Button(label)) {
                    state.vegetationDeltaPct = vegPresets[i];
                    changed = true;
                }
            }
            float prevVeg = state.vegetationDeltaPct;
            float vegPercent = state.vegetationDeltaPct * 100.0f;
            if (ImGui::SliderFloat("Vegetation Increase", &vegPercent, 0.0f, 30.0f, "+%.0f%%")) {
                state.vegetationDeltaPct = vegPercent / 100.0f;
                changed = true;
            }

            ImGui::Spacing();
            ImGui::TextUnformatted("Add Shaded Areas / Reflective Roofs:");
            const float shadePresets[] = { 0.05f, 0.10f, 0.20f };
            for (int i = 0; i < 3; ++i) {
                if (i > 0) ImGui::SameLine();
                char label[16];
                std::snprintf(label, sizeof(label), "+%d%%", static_cast<int>(shadePresets[i] * 100.0f));
                if (ImGui::Button(label)) {
                    state.shadeDeltaPct = shadePresets[i];
                    changed = true;
                }
            }
            float prevShade = state.shadeDeltaPct;
            float shadePercent = state.shadeDeltaPct * 100.0f;
            if (ImGui::SliderFloat("Shade Increase", &shadePercent, 0.0f, 30.0f, "+%.0f%%")) {
                state.shadeDeltaPct = shadePercent / 100.0f;
                changed = true;
            }
        }

        // --- RESULTS & METRICS DISPLAY ---
        const float scenarioTemperature = baselineTemperatureC +
            (state.heatwaveEnabled ? state.temperatureIncreaseC : 0.0f);
        ScenarioMetrics metrics = Measure(zones);

        ImGui::Separator();
        ImGui::Text("Baseline Temp: %.1f C", baselineTemperatureC);
        ImGui::SameLine();
        ImGui::Text("Scenario Temp: %.1f C", scenarioTemperature);
        ImGui::Text("Critical zones: %d", metrics.criticalZones);
        ImGui::SameLine();
        ImGui::Text("Exposed pop: %d", metrics.exposedPopulation);
        ImGui::SameLine();
        ImGui::Text("High-risk: %d", metrics.highRiskPopulation);

        if (state.interventionEnabled) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.20f, 0.85f, 0.40f, 1.0f), "MODELLED WHAT-IF COMPARISON");

            // Look up selected zone or city average
            const Zone* targetZone = nullptr;
            if (state.applyToSelectedZoneOnly && selectedZoneId >= 0) {
                for (const auto& z : zones) {
                    if (z.id == selectedZoneId) { targetZone = &z; break; }
                }
            }

            float beforeRisk = targetZone
                ? (targetZone->baselineHeatRisk > 0.0f ? targetZone->baselineHeatRisk : targetZone->heatRisk)
                : metrics.avgHeatRiskBefore;
            float afterRisk = targetZone ? targetZone->heatRisk : metrics.avgHeatRiskAfter;
            float riskDelta = afterRisk - beforeRisk;

            ImGui::Columns(3, "interventionCompare", false);
            ImGui::Text("BEFORE\n%.0f", beforeRisk); ImGui::NextColumn();
            ImGui::Text("AFTER\n%.0f", afterRisk); ImGui::NextColumn();
            if (riskDelta < 0.0f) {
                ImGui::TextColored(ImVec4(0.30f, 0.85f, 0.30f, 1.0f), "CHANGE\n%.0f pts", riskDelta);
            } else if (riskDelta > 0.0f) {
                ImGui::TextColored(ImVec4(0.90f, 0.30f, 0.30f, 1.0f), "CHANGE\n+%.0f pts", riskDelta);
            } else {
                ImGui::Text("CHANGE\n0 pts");
            }
            ImGui::Columns(1);

            ImGui::TextDisabled("MODELLED SCENARIO ESTIMATE");
            ImGui::TextDisabled("Prototype decision-support estimate — not a scientifically validated real-world reduction.");
        } else {
            ImGui::TextDisabled("Scores update immediately. Prototype modelled scenario estimate.");
        }

        if (state.floodEnabled) {
            ImGui::Text("Rainfall: %.0f mm (current %.1f mm)", state.rainfallScenarioMm, baselineRainfallMm);
            ImGui::Text("Potentially affected population: %d", metrics.floodAffectedPopulation);
            ImGui::TextDisabled("Flood risk uses rainfall plus OSM surface proxies; no hydrological model.");
        }

        ImGui::End();
        return changed;
    }

}  // namespace twin
