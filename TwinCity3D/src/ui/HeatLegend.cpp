#include "HeatLegend.h"

#include <iamgui/imgui.h>

namespace twin {

    namespace {
        // Must match basic.frag's DataRamp() exactly — see HeatLegend.h's
        // doc comment on why this can't be a single shared source today.
        constexpr ImVec4 kLegendStops[5] = {
            ImVec4(0.20f, 0.40f, 0.85f, 1.0f),  // low
            ImVec4(0.20f, 0.70f, 0.55f, 1.0f),
            ImVec4(0.90f, 0.85f, 0.20f, 1.0f),
            ImVec4(0.95f, 0.55f, 0.15f, 1.0f),
            ImVec4(0.85f, 0.15f, 0.15f, 1.0f),  // high
        };

        // Matches HeatRiskModel::Classify's actual riskClass strings (see
        // Zone.h) — not the master spec mock-up's "COOL/.../EXTREME"
        // wording, so this panel never introduces a term the rest of the
        // app doesn't already use (Inspector shows the same "Low" /
        // "Moderate" / "High" / "Very High" / "Extreme" labels).
        constexpr const char* kHeatRiskBands[5] = {
            "Low", "Moderate", "High", "Very High", "Extreme"
        };

        void DrawGradientBar(float width, float height) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 origin = ImGui::GetCursorScreenPos();
            float stopWidth = width / 5.0f;

            for (int i = 0; i < 5; ++i) {
                ImVec2 topLeft(origin.x + stopWidth * i, origin.y);
                ImVec2 bottomRight(origin.x + stopWidth * (i + 1), origin.y + height);
                drawList->AddRectFilled(topLeft, bottomRight, ImGui::ColorConvertFloat4ToU32(kLegendStops[i]));
            }
            drawList->AddRect(origin, ImVec2(origin.x + width, origin.y + height),
                               ImGui::ColorConvertFloat4ToU32(ImVec4(0.0f, 0.0f, 0.0f, 0.5f)));

            ImGui::Dummy(ImVec2(width, height));
        }
    }  // namespace

    void HeatLegend::Render(DataLayer activeLayer) {
        ImGui::SetNextWindowSize(ImVec2(260.0f, 0.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(980.0f, 20.0f), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Legend")) {
            ImGui::End();
            return;
        }

        LayerInfo info = DescribeLayer(activeLayer);
        ImGui::Text("%s", info.name.c_str());
        if (!info.units.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", info.units.c_str());
        }
        ImGui::Spacing();

        const float barWidth = 220.0f;
        DrawGradientBar(barWidth, 18.0f);

        if (activeLayer == DataLayer::HeatRisk) {
            // 5 band names, evenly split under the gradient bar.
            ImGui::Columns(5, "heatLegendBands", false);
            for (int i = 0; i < 5; ++i) {
                ImGui::TextDisabled("%s", kHeatRiskBands[i]);
                ImGui::NextColumn();
            }
            ImGui::Columns(1);
        } else {
            ImGui::TextDisabled("%s", info.lowLabel.c_str());
            ImGui::SameLine(barWidth - 60.0f);
            ImGui::TextDisabled("%s", info.highLabel.c_str());
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Gray = no data yet for this zone");

        ImGui::End();
    }

}  // namespace twin
