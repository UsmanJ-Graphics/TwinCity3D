#include "ScenarioPanel.h"

#include <iamgui/imgui.h>
#include <cstdio>

namespace twin {

    namespace {
        struct ScenarioMetrics {
            int criticalZones{ 0 };
            int highRiskPopulation{ 0 };
            int extremeRiskPopulation{ 0 };
            int floodAffectedPopulation{ 0 };
        };

        ScenarioMetrics Measure(const std::vector<Zone>& zones) {
            ScenarioMetrics result;
            for (const Zone& zone : zones) {
                if (zone.riskIsPlaceholder || zone.populationIsPlaceholder) continue;
                if (zone.heatRisk >= 70.0f) {
                    ++result.criticalZones;
                    result.highRiskPopulation += zone.population;
                }
                if (zone.heatRisk >= 90.0f) result.extremeRiskPopulation += zone.population;
                if (!zone.floodRiskIsPlaceholder && zone.floodRisk >= 50.0f)
                    result.floodAffectedPopulation += zone.population;
            }
            return result;
        }
    }

    bool ScenarioPanel::Render(ScenarioState& state, const std::vector<Zone>& zones,
        float baselineTemperatureC, float baselineRainfallMm) {
        bool changed = false;
        ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(300.0f, 510.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Scenario Control")) {
            ImGui::End();
            return false;
        }

        ImGui::Text("SCENARIO");
        ImGui::SameLine();
        if (ImGui::RadioButton("Current Conditions", !state.heatwaveEnabled)) {
            state.heatwaveEnabled = false;
            state.floodEnabled = false;
            state.temperatureIncreaseC = 0.0f;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Heatwave", state.heatwaveEnabled)) {
            state.heatwaveEnabled = true;
            state.floodEnabled = false;
            if (state.temperatureIncreaseC <= 0.0f) state.temperatureIncreaseC = 1.0f;
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Flood", state.floodEnabled)) {
            state.floodEnabled = true;
            state.heatwaveEnabled = false;
            state.rainfallScenarioMm = 80.0f;
            changed = true;
        }

        if (state.heatwaveEnabled) {
            ImGui::Separator();
            ImGui::Text("Temperature increase");
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
        if (state.floodEnabled) {
            ImGui::Separator();
            ImGui::Text("Rainfall scenario");
            if (ImGui::Button("Simulate Heavy Rainfall (80 mm)")) {
                state.rainfallScenarioMm = 80.0f;
                changed = true;
            }
            float previous = state.rainfallScenarioMm;
            ImGui::SliderFloat("Rainfall", &state.rainfallScenarioMm, 0.0f, 150.0f, "%.0f mm");
            if (previous != state.rainfallScenarioMm) changed = true;
        }

        const float scenarioTemperature = baselineTemperatureC +
            (state.heatwaveEnabled ? state.temperatureIncreaseC : 0.0f);
        ScenarioMetrics metrics = Measure(zones);
        ImGui::Separator();
        ImGui::Text("Baseline: %.1f C", baselineTemperatureC);
        ImGui::SameLine();
        ImGui::Text("Scenario: %.1f C", scenarioTemperature);
        ImGui::Text("Critical zones: %d", metrics.criticalZones);
        ImGui::SameLine();
        ImGui::Text("High-risk population: %d", metrics.highRiskPopulation);
        ImGui::SameLine();
        ImGui::Text("Extreme: %d", metrics.extremeRiskPopulation);
        ImGui::TextDisabled("Scores update immediately. Prototype modelled scenario estimate.");
        if (state.floodEnabled) {
            ImGui::Text("Rainfall: %.0f mm (current %.1f mm)", state.rainfallScenarioMm, baselineRainfallMm);
            ImGui::Text("Potentially affected population: %d", metrics.floodAffectedPopulation);
            ImGui::TextDisabled("Flood risk uses rainfall plus OSM surface proxies; no hydrological model.");
        }

        ImGui::End();
        return changed;
    }

}  // namespace twin
