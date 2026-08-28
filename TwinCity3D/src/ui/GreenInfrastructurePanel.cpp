#include "GreenInfrastructurePanel.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include <iamgui/imgui.h>

namespace twin {

    namespace {
        // Colour for the deficit / priority bars — a green-to-red ramp
        // that matches the existing DataRamp direction (cold=good/green,
        // hot=bad/red) so bar colours feel consistent with the 3D view.
        ImVec4 GreenDeficitColor(float deficit01) {
            // deficit 0 => bright green, deficit 1 => orange-red
            float r = std::min(1.0f, 2.0f * deficit01);
            float g = std::min(1.0f, 2.0f * (1.0f - deficit01));
            return ImVec4(r, g, 0.10f, 1.0f);
        }

        ImVec4 GreenPriorityColor(float priority01) {
            float r = std::min(1.0f, 2.0f * priority01);
            float g = std::min(1.0f, 2.0f * (1.0f - priority01));
            return ImVec4(r, g, 0.15f, 1.0f);
        }

        // Inline horizontal progress bar using the draw list (no ImGui::ProgressBar
        // so we can control colour independently of the current style).
        void ColoredBar(float value01, float width, float height, ImVec4 color) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(p,
                ImVec2(p.x + width * value01, p.y + height),
                ImGui::ColorConvertFloat4ToU32(color));
            dl->AddRect(p, ImVec2(p.x + width, p.y + height),
                IM_COL32(80, 80, 80, 180));
            ImGui::Dummy(ImVec2(width, height));
        }
    }  // namespace

    int GreenInfrastructurePanel::Render(const std::vector<Zone>& zones, int maxEntries) {
        ImGui::SetNextWindowSize(ImVec2(300.0f, 0.0f), ImGuiCond_FirstUseEver);
        // Position below the Top Priority Zones panel (which sits at ~340 + its content)
        ImGui::SetNextWindowPos(ImVec2(20.0f, 590.0f), ImGuiCond_FirstUseEver);

        int clickedZoneId = -1;

        if (!ImGui::Begin("Green Infrastructure")) {
            ImGui::End();
            return clickedZoneId;
        }

        // ---------------------------------------------------------------
        // Study-area summary
        // ---------------------------------------------------------------
        float totalCoverage = 0.0f;
        float totalDeficit  = 0.0f;
        int   zonesWithData = 0;

        for (const auto& z : zones) {
            totalCoverage += z.greenCoverage;
            if (!z.greenInfraIsPlaceholder) {
                totalDeficit += z.greenDeficit;
                ++zonesWithData;
            }
        }

        const int n = static_cast<int>(zones.size());
        const float meanCoverage = (n > 0) ? totalCoverage / n : 0.0f;
        const float meanDeficit  = (zonesWithData > 0) ? totalDeficit / zonesWithData : 0.0f;

        ImGui::TextUnformatted("GREEN COVERAGE");
        ImGui::Separator();

        char buf[64];
        std::snprintf(buf, sizeof(buf), "Study-area mean: %.1f%%", meanCoverage * 100.0f);
        ImGui::TextUnformatted(buf);

        // Shade the bar green below 20 % (target), red above — a simple visual
        // indicator that most of Lahore sits well below the 20 % urban target.
        ColoredBar(std::min(meanCoverage / 0.20f, 1.0f), 220.0f, 12.0f,
            ImVec4(0.20f, 0.65f, 0.25f, 0.90f));

        std::snprintf(buf, sizeof(buf), "Target: 20%%   Deficit: %.1f%%",
            std::max(0.0f, 0.20f - meanCoverage) * 100.0f);
        ImGui::TextDisabled("%s", buf);

        ImGui::Spacing();
        ImGui::TextDisabled("Prototype modelled scenario estimate.");

        if (zonesWithData == 0) {
            ImGui::TextWrapped("Green infrastructure scores not yet computed "
                "(requires BuildGreenInfrastructure step).");
            ImGui::End();
            return clickedZoneId;
        }

        // ---------------------------------------------------------------
        // Top green-priority zones
        // ---------------------------------------------------------------
        ImGui::Spacing();
        ImGui::TextUnformatted("WHERE TO ADD GREENERY");
        ImGui::Separator();
        ImGui::TextDisabled("Zones with greatest green-infrastructure need:");

        // Gather and sort by greenPriorityRank (1 = most urgent, -1 = unranked).
        std::vector<const Zone*> ranked;
        for (const auto& z : zones) {
            if (!z.greenInfraIsPlaceholder && z.greenPriorityRank > 0)
                ranked.push_back(&z);
        }
        std::sort(ranked.begin(), ranked.end(),
            [](const Zone* a, const Zone* b) { return a->greenPriorityRank < b->greenPriorityRank; });

        const float barWidth = 160.0f;
        const float barHeight = 10.0f;

        int shown = 0;
        for (const Zone* z : ranked) {
            if (shown >= maxEntries) break;
            ++shown;

            // Row header
            char label[80];
            std::snprintf(label, sizeof(label), "#%d  Zone %d",
                z->greenPriorityRank, z->id);

            bool isSelected = false;  // highlight handled by Application
            if (ImGui::Selectable(label, isSelected, 0, ImVec2(0, 0))) {
                clickedZoneId = z->id;
            }

            // Green priority bar
            ImGui::SameLine(150.0f);
            ImGui::TextDisabled("%.0f", z->greenPriority);

            // Deficit sub-bar
            ImGui::Indent(12.0f);

            ImGui::TextDisabled("Coverage: %.1f%%  Deficit:", z->greenCoverage * 100.0f);
            ImGui::SameLine();
            ColoredBar(z->greenDeficit, barWidth * 0.7f, barHeight,
                GreenDeficitColor(z->greenDeficit));

            // Benefit estimate
            if (!z->greenInfraIsPlaceholder && z->greenBenefitScore > 0.0f) {
                std::snprintf(buf, sizeof(buf),
                    "Benefit potential: %.0f%%", z->greenBenefitScore * 100.0f);
                ImGui::TextDisabled("%s", buf);
            }

            ImGui::Unindent(12.0f);
            ImGui::Spacing();
        }

        // ---------------------------------------------------------------
        // Zone-by-zone deficit table (all zones, sorted by deficit)
        // ---------------------------------------------------------------
        ImGui::Spacing();
        if (ImGui::CollapsingHeader("All zones (deficit ranking)")) {
            std::vector<const Zone*> all;
            for (const auto& z : zones) {
                if (!z.greenInfraIsPlaceholder) all.push_back(&z);
            }
            std::sort(all.begin(), all.end(),
                [](const Zone* a, const Zone* b) { return a->greenDeficit > b->greenDeficit; });

            ImGui::Columns(3, "greenTable", true);
            ImGui::TextDisabled("Zone");  ImGui::NextColumn();
            ImGui::TextDisabled("Coverage"); ImGui::NextColumn();
            ImGui::TextDisabled("Deficit");  ImGui::NextColumn();
            ImGui::Separator();

            for (const Zone* z : all) {
                std::snprintf(buf, sizeof(buf), "%d", z->id);
                ImGui::TextUnformatted(buf);
                ImGui::NextColumn();

                std::snprintf(buf, sizeof(buf), "%.1f%%", z->greenCoverage * 100.0f);
                ImGui::TextColored(
                    ImVec4(0.30f, 0.75f, 0.30f, 1.0f), "%s", buf);
                ImGui::NextColumn();

                ColoredBar(z->greenDeficit, 80.0f, 10.0f,
                    GreenDeficitColor(z->greenDeficit));
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Click a zone to focus and inspect.");
        ImGui::TextDisabled("Prototype modelled scenario estimate.");

        ImGui::End();
        return clickedZoneId;
    }

}  // namespace twin
