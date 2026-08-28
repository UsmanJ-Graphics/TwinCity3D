#include "Inspector.h"

#include <cstdio>
#include <string>

#include <iamgui/imgui.h>

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
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("PRIORITY");
        // Priority ranking is Phase 11 — until then this is honestly labeled
        // as not-yet-computed rather than showing a stray 0, which would
        // read as "this zone has zero priority" instead of "not ranked yet."
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Not yet ranked (Phase 11)");

        ImGui::Spacing();
        ImGui::TextDisabled("Prototype decision-support score, not a validated");
        ImGui::TextDisabled("medical or scientific heat-risk measurement.");
    }

}  // namespace twin
