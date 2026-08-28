#include "LayersPanel.h"

#include <cstdio>
#include <iamgui/imgui.h>

namespace twin {

    namespace {
        struct LayerEntry {
            DataLayer layer;
            const char* label;
        };

        // Same order as the Phase 9 keyboard shortcuts (1=HeatRisk,
        // 2=Population, 3=GreenCoverage, 4=BuildingDensity, 5=Temperature)
        // so the panel and the hotkey hint below it describe the same
        // ordering, not two different mental models for the same feature.
        constexpr LayerEntry kEntries[] = {
            { DataLayer::HeatRisk,        "Heat Risk" },
            { DataLayer::Population,      "Population" },
            { DataLayer::PopulationExposure, "Population Exposure" },
            { DataLayer::GreenCoverage,   "Green Coverage" },
            { DataLayer::BuildingDensity, "Building Density" },
            { DataLayer::Temperature,     "Temperature" },
            { DataLayer::FloodRisk,       "Flood Risk" },
            { DataLayer::SatelliteEnvironment, "Environment (proxy)" },
        };
    }  // namespace

    std::optional<DataLayer> LayersPanel::Render(DataLayer activeLayer) {
        ImGui::SetNextWindowSize(ImVec2(220.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(20.0f, 640.0f), ImGuiCond_FirstUseEver);

        std::optional<DataLayer> clicked;

        if (!ImGui::Begin("City Layers")) {
            ImGui::End();
            return clicked;
        }

        ImGui::TextDisabled("Colors the 3D twin below");
        ImGui::Separator();

        for (const auto& entry : kEntries) {
            bool isActive = (entry.layer == activeLayer);

            // Plain-ASCII checkbox glyph rather than ImGui::Checkbox: a
            // real checkbox widget wants an independent bool per row, which
            // would let the UI *look* like several layers could be checked
            // at once — misleading, since there's only ever one
            // m_activeLayer (see class doc comment). A single-select
            // Selectable with a "[x]"/"[ ]" label prefix gives the same
            // checkbox-style visual from the master spec's mock-up while
            // keeping the underlying single-select contract honest.
            char label[64];
            std::snprintf(label, sizeof(label), "%s  %s", isActive ? "[x]" : "[ ]", entry.label);

            if (ImGui::Selectable(label, isActive)) {
                if (!isActive) clicked = entry.layer;
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Keys [1-8] or [L] also switch layers.");

        ImGui::End();
        return clicked;
    }

}  // namespace twin
