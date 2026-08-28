#include "CommandCenterUI.h"
#include "../twin/HeatRiskModel.h"
#include "../twin/GreenInfrastructureModel.h"

#include <iamgui/imgui.h>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>

namespace twin {

    namespace {
        // Color Palette matching the professional dark Command Center design
        constexpr ImVec4 kBgColor       = ImVec4(0.06f, 0.09f, 0.13f, 0.94f);
        constexpr ImVec4 kPanelBg       = ImVec4(0.09f, 0.14f, 0.20f, 0.92f);
        constexpr ImVec4 kHeaderBg      = ImVec4(0.12f, 0.19f, 0.28f, 1.00f);
        constexpr ImVec4 kAccentRed     = ImVec4(0.90f, 0.25f, 0.25f, 1.00f);
        constexpr ImVec4 kAccentOrange  = ImVec4(0.95f, 0.55f, 0.15f, 1.00f);
        constexpr ImVec4 kAccentYellow  = ImVec4(0.90f, 0.85f, 0.20f, 1.00f);
        constexpr ImVec4 kAccentGreen   = ImVec4(0.20f, 0.75f, 0.40f, 1.00f);
        constexpr ImVec4 kAccentCyan    = ImVec4(0.15f, 0.70f, 0.85f, 1.00f);
        constexpr ImVec4 kTextMuted     = ImVec4(0.65f, 0.72f, 0.80f, 1.00f);

        // Helper to draw a sleek horizontal colored bar
        void DrawProgressBar(float value01, float width, float height, ImVec4 color) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float fillWidth = std::clamp(value01, 0.0f, 1.0f) * width;
            dl->AddRectFilled(p, ImVec2(p.x + fillWidth, p.y + height), ImGui::ColorConvertFloat4ToU32(color), 3.0f);
            dl->AddRect(p, ImVec2(p.x + width, p.y + height), IM_COL32(80, 100, 120, 150), 3.0f);
            ImGui::Dummy(ImVec2(width, height));
        }



        // Color for risk scores 0..100
        ImVec4 RiskColor(float risk0to100) {
            if (risk0to100 <= 30.0f) return kAccentCyan;
            if (risk0to100 <= 50.0f) return kAccentGreen;
            if (risk0to100 <= 70.0f) return kAccentYellow;
            if (risk0to100 <= 85.0f) return kAccentOrange;
            return kAccentRed;
        }
    }  // namespace

    void CommandCenterUI::InitStyle() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 6.0f;
        style.ChildRounding = 6.0f;
        style.FrameRounding = 4.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 4.0f;

        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
        style.PopupBorderSize = 1.0f;

        style.WindowPadding = ImVec2(10.0f, 10.0f);
        style.FramePadding = ImVec2(8.0f, 5.0f);
        style.ItemSpacing = ImVec2(8.0f, 6.0f);
        style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);

        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg]             = kBgColor;
        colors[ImGuiCol_ChildBg]              = kPanelBg;
        colors[ImGuiCol_PopupBg]              = kBgColor;
        colors[ImGuiCol_Border]               = ImVec4(0.20f, 0.30f, 0.42f, 0.60f);
        colors[ImGuiCol_BorderShadow]         = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]              = ImVec4(0.12f, 0.18f, 0.26f, 0.80f);
        colors[ImGuiCol_FrameBgHovered]       = ImVec4(0.18f, 0.26f, 0.36f, 0.90f);
        colors[ImGuiCol_FrameBgActive]        = ImVec4(0.22f, 0.32f, 0.44f, 1.00f);
        colors[ImGuiCol_TitleBg]              = kHeaderBg;
        colors[ImGuiCol_TitleBgActive]        = kHeaderBg;
        colors[ImGuiCol_TitleBgCollapsed]     = kHeaderBg;
        colors[ImGuiCol_MenuBarBg]            = kHeaderBg;
        colors[ImGuiCol_ScrollbarBg]          = ImVec4(0.08f, 0.12f, 0.18f, 0.60f);
        colors[ImGuiCol_ScrollbarGrab]        = ImVec4(0.20f, 0.30f, 0.42f, 0.80f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.42f, 0.56f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.40f, 0.55f, 0.70f, 1.00f);
        colors[ImGuiCol_CheckMark]            = kAccentCyan;
        colors[ImGuiCol_SliderGrab]           = kAccentCyan;
        colors[ImGuiCol_SliderGrabActive]     = kAccentGreen;
        colors[ImGuiCol_Button]               = ImVec4(0.14f, 0.22f, 0.32f, 0.85f);
        colors[ImGuiCol_ButtonHovered]        = ImVec4(0.22f, 0.34f, 0.48f, 1.00f);
        colors[ImGuiCol_ButtonActive]         = ImVec4(0.18f, 0.45f, 0.65f, 1.00f);
        colors[ImGuiCol_Header]               = ImVec4(0.15f, 0.24f, 0.35f, 0.80f);
        colors[ImGuiCol_HeaderHovered]        = ImVec4(0.22f, 0.34f, 0.48f, 0.90f);
        colors[ImGuiCol_HeaderActive]         = ImVec4(0.18f, 0.45f, 0.65f, 1.00f);
        colors[ImGuiCol_Text]                 = ImVec4(0.92f, 0.95f, 0.98f, 1.00f);
        colors[ImGuiCol_TextDisabled]         = kTextMuted;
    }

    UICommandResult CommandCenterUI::Render(
        DataLayer activeLayer,
        CameraMode cameraMode,
        const Zone* selectedZone,
        const std::vector<Zone>& zones,
        ScenarioState& scenarioState,
        const WeatherData& weather,
        const PopulationData& population,
        int displayWidth,
        int displayHeight
    ) {
        UICommandResult result;

        const float topBarHeight = 44.0f;
        const float leftSidebarWidth = 200.0f;
        const float rightPanelWidth = 310.0f;
        const float bottomDashboardHeight = 150.0f;
        const float footerHeight = 24.0f;

        const float mainAreaY = topBarHeight;
        const float mainAreaHeight = displayHeight - topBarHeight - footerHeight;

        const ImGuiWindowFlags dockedFlags =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoBringToFrontOnFocus;

        // -----------------------------------------------------------------
        // 1. TOP BAR (DYNAMIC RELATIVE SPACING - NO TEXT OVERLAP)
        // -----------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(displayWidth), topBarHeight));
        if (ImGui::Begin("##TopBar", nullptr, dockedFlags)) {
            ImGui::TextColored(ImVec4(0.30f, 0.85f, 1.00f, 1.0f), "LAHORE ");
            ImGui::SameLine(0, 0);
            ImGui::TextUnformatted("URBAN DIGITAL TWIN");
            ImGui::SameLine(0, 10.0f);
            ImGui::TextDisabled("|");

            // Relative status badges
            ImGui::SameLine(0, 10.0f);
            ImGui::TextColored(kAccentGreen, "LIVE");
            ImGui::SameLine(0, 10.0f);
            ImGui::Text("Temp: %.1f C", weather.valid ? weather.currentTemperature : 34.5f);
            ImGui::SameLine(0, 10.0f);
            ImGui::TextColored(kAccentRed, "Heat Risk: HIGH");
            ImGui::SameLine(0, 10.0f);
            ImGui::TextDisabled("29 Aug 2026");

            // Right Camera Controls
            float camX = std::max(550.0f, static_cast<float>(displayWidth) - 300.0f);
            ImGui::SameLine(camX);
            ImGui::TextDisabled("Cam:");
            ImGui::SameLine(0, 4.0f);
            if (ImGui::Button(cameraMode == CameraMode::FreeFly ? "[F] FreeFly" : "FreeFly"))
                result.newCameraMode = CameraMode::FreeFly;
            ImGui::SameLine(0, 3.0f);
            if (ImGui::Button(cameraMode == CameraMode::Orbit ? "[O] Orbit" : "Orbit"))
                result.newCameraMode = CameraMode::Orbit;
            ImGui::SameLine(0, 3.0f);
            if (ImGui::Button(cameraMode == CameraMode::TopDown ? "[T] 2D" : "2D"))
                result.newCameraMode = CameraMode::TopDown;
            ImGui::SameLine(0, 3.0f);
            if (ImGui::Button(cameraMode == CameraMode::Isometric ? "[I] Iso" : "Iso"))
                result.newCameraMode = CameraMode::Isometric;
        }
        ImGui::End();

        // -----------------------------------------------------------------
        // 2. LEFT NAVIGATION SIDEBAR (DOCKED ON LEFT)
        // -----------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(0.0f, mainAreaY));
        ImGui::SetNextWindowSize(ImVec2(leftSidebarWidth, mainAreaHeight));
        if (ImGui::Begin("##LeftSidebar", nullptr, dockedFlags)) {
            ImGui::TextColored(kTextMuted, "CITY LAYERS");
            ImGui::Separator();

            struct NavItem { DataLayer layer; const char* iconLabel; };
            constexpr NavItem navItems[] = {
                { DataLayer::HeatRisk,            "Heat Risk" },
                { DataLayer::Population,          "Population" },
                { DataLayer::PopulationExposure,  "Pop Exposure" },
                { DataLayer::GreenCoverage,       "Green Coverage" },
                { DataLayer::BuildingDensity,     "Building Density" },
                { DataLayer::Temperature,         "Temperature" },
                { DataLayer::FloodRisk,           "Flood Risk" },
                { DataLayer::SatelliteEnvironment,"Satellite Env" },
                { DataLayer::GreenPriority,       "Green Priority" },
                { DataLayer::LightPollution,      "Light Pollution" },
                { DataLayer::BirdEcologicalImpact,"Bird & Ecology" },
            };

            for (const auto& item : navItems) {
                bool selected = (activeLayer == item.layer);
                char label[64];
                std::snprintf(label, sizeof(label), "%s %s", selected ? ">" : " ", item.iconLabel);
                if (ImGui::Selectable(label, selected)) {
                    result.newActiveLayer = item.layer;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(kTextMuted, "SCENARIOS");

            bool isScenarioActive = scenarioState.heatwaveEnabled || scenarioState.floodEnabled || scenarioState.interventionEnabled;
            if (ImGui::Selectable(isScenarioActive ? "> Scenarios & What-If" : "  Scenarios & What-If", isScenarioActive)) {
                if (!isScenarioActive) {
                    scenarioState.interventionEnabled = true;
                    result.scenarioStateChanged = true;
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextDisabled("STUDY AREA");
            ImGui::TextUnformatted("Gulberg III Corridor");
            ImGui::TextDisabled("Lahore, Pakistan");
        }
        ImGui::End();

        // -----------------------------------------------------------------
        // 3. RIGHT INSPECTOR & PRIORITY PANEL (NATURAL SEQUENTIAL FLOW)
        // -----------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(static_cast<float>(displayWidth) - rightPanelWidth, mainAreaY));
        ImGui::SetNextWindowSize(ImVec2(rightPanelWidth, mainAreaHeight));
        if (ImGui::Begin("##RightInspector", nullptr, dockedFlags)) {
            if (selectedZone) {
                ImGui::TextColored(kAccentCyan, "ZONE %d", selectedZone->id);
                ImGui::SameLine(110.0f);
                ImGui::TextDisabled("Shadman / Gulberg");

                if (!selectedZone->priorityIsPlaceholder) {
                    char badge[64];
                    std::snprintf(badge, sizeof(badge), "#%d PRIORITY (%.0f pts)",
                        selectedZone->priorityRank, selectedZone->priority);
                    ImGui::TextColored(RiskColor(selectedZone->priority), "%s", badge);
                }
                ImGui::Separator();

                // Heat Risk Score Card
                ImGui::Spacing();
                ImGui::TextUnformatted("HEAT RISK SCORE");
                if (selectedZone->riskIsPlaceholder) {
                    ImGui::TextDisabled("-- / 100 (Incomplete Data)");
                } else {
                    ImGui::TextColored(RiskColor(selectedZone->heatRisk), "%.0f / 100", selectedZone->heatRisk);
                    ImGui::SameLine(130.0f);
                    ImGui::TextColored(RiskColor(selectedZone->heatRisk), "(%s)", selectedZone->riskClass.c_str());
                }

                // Detailed Metrics Grid
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("METRICS");

                ImGui::Text("Temperature:");
                ImGui::SameLine(150.0f);
                if (selectedZone->temperatureIsPlaceholder) ImGui::TextDisabled("--");
                else ImGui::Text("%.1f C", selectedZone->temperature);

                ImGui::Text("Population:");
                ImGui::SameLine(150.0f);
                if (selectedZone->populationIsPlaceholder) ImGui::TextDisabled("--");
                else ImGui::Text("%d", selectedZone->population);

                ImGui::Text("Pop Density:");
                ImGui::SameLine(150.0f);
                if (selectedZone->populationIsPlaceholder) ImGui::TextDisabled("--");
                else ImGui::Text("%.0f /km^2", selectedZone->populationDensity);

                ImGui::Text("Green Coverage:");
                ImGui::SameLine(150.0f);
                ImGui::Text("%.1f%%", selectedZone->greenCoverage * 100.0f);

                ImGui::Text("Building Density:");
                ImGui::SameLine(150.0f);
                ImGui::Text("%.1f%%", selectedZone->buildingDensity * 100.0f);

                ImGui::Text("Light Pollution:");
                ImGui::SameLine(150.0f);
                if (selectedZone->ecologicalIsPlaceholder) ImGui::TextDisabled("--");
                else ImGui::Text("%.2f / 1.0", selectedZone->lightPollutionIndex);

                ImGui::Text("Bird Disturbance:");
                ImGui::SameLine(150.0f);
                if (selectedZone->ecologicalIsPlaceholder) ImGui::TextDisabled("--");
                else ImGui::Text("%.0f / 100", selectedZone->birdEcologicalDisturbance);

                // What-If Comparison if active
                if (selectedZone->baselineHeatRisk > 0.0f && std::fabs(selectedZone->heatRisk - selectedZone->baselineHeatRisk) >= 0.1f) {
                    float delta = selectedZone->heatRisk - selectedZone->baselineHeatRisk;
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextColored(kAccentGreen, "WHAT-IF INTERVENTION");
                    ImGui::Text("Before: %.0f", selectedZone->baselineHeatRisk);
                    ImGui::SameLine(100.0f);
                    ImGui::Text("After: %.0f", selectedZone->heatRisk);
                    ImGui::SameLine(180.0f);
                    if (delta < 0) ImGui::TextColored(kAccentGreen, "(%.0f pts)", delta);
                    else ImGui::TextColored(kAccentRed, "(+%.0f pts)", delta);
                    ImGui::TextDisabled("MODELLED SCENARIO ESTIMATE");
                }

                // Why This Zone Breakdown
                if (!selectedZone->priorityIsPlaceholder) {
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::TextDisabled("WHY THIS ZONE?");
                    ImGui::BulletText("Heat risk: %.0f / 100 (%s)", selectedZone->heatRisk, selectedZone->riskClass.c_str());
                    ImGui::BulletText("Green deficit: %.1f%%", (1.0f - selectedZone->greenCoverage) * 100.0f);
                    ImGui::BulletText("Building density: %.1f%%", selectedZone->buildingDensity * 100.0f);
                    ImGui::BulletText("Population: %d residents", selectedZone->population);
                }
            } else {
                ImGui::TextColored(kAccentCyan, "ZONE INTELLIGENCE");
                ImGui::Separator();
                ImGui::TextWrapped("Click any zone in the 3D digital twin to inspect temperature, risk, and intervention metrics.");
            }

            // Top Priority Zones List - natural sequential flow
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(kAccentRed, "TOP PRIORITY ZONES");

            std::vector<const Zone*> ranked;
            for (const auto& z : zones) {
                if (!z.priorityIsPlaceholder) ranked.push_back(&z);
            }
            std::sort(ranked.begin(), ranked.end(),
                [](const Zone* a, const Zone* b) { return a->priorityRank < b->priorityRank; });

            int shown = 0;
            for (const Zone* z : ranked) {
                if (shown >= 4) break;
                ++shown;

                char label[64];
                std::snprintf(label, sizeof(label), "#%d  Zone %d", z->priorityRank, z->id);
                if (ImGui::Selectable(label)) {
                    result.focusedZoneId = z->id;
                }
                ImGui::SameLine(110.0f);
                DrawProgressBar(z->priority / 100.0f, 110.0f, 10.0f, RiskColor(z->priority));
                ImGui::SameLine(235.0f);
                ImGui::Text("%.0f", z->priority);
            }
        }
        ImGui::End();

        // -----------------------------------------------------------------
        // 4. BOTTOM ANALYTICS & SCENARIO DASHBOARD
        // -----------------------------------------------------------------
        const float bottomX = leftSidebarWidth;
        const float bottomWidth = static_cast<float>(displayWidth) - leftSidebarWidth - rightPanelWidth;
        const float bottomY = static_cast<float>(displayHeight) - footerHeight - bottomDashboardHeight;

        ImGui::SetNextWindowPos(ImVec2(bottomX, bottomY));
        ImGui::SetNextWindowSize(ImVec2(bottomWidth, bottomDashboardHeight));
        if (ImGui::Begin("##BottomDashboard", nullptr, dockedFlags)) {
            if (scenarioState.interventionEnabled || scenarioState.heatwaveEnabled || scenarioState.floodEnabled) {
                // SCENARIO TYPE SELECTOR RADIO BUTTONS
                ImGui::TextColored(kAccentGreen, "SIMULATION SCENARIO:");
                ImGui::SameLine(0, 12.0f);

                bool isCurrent = !scenarioState.heatwaveEnabled && !scenarioState.floodEnabled && !scenarioState.interventionEnabled;
                if (ImGui::RadioButton("Current", isCurrent)) {
                    scenarioState.heatwaveEnabled = false;
                    scenarioState.floodEnabled = false;
                    scenarioState.interventionEnabled = false;
                    scenarioState.temperatureIncreaseC = 0.0f;
                    scenarioState.vegetationDeltaPct = 0.0f;
                    scenarioState.shadeDeltaPct = 0.0f;
                    result.scenarioStateChanged = true;
                }
                ImGui::SameLine(0, 10.0f);
                if (ImGui::RadioButton("Heatwave", scenarioState.heatwaveEnabled)) {
                    scenarioState.heatwaveEnabled = true;
                    scenarioState.floodEnabled = false;
                    scenarioState.interventionEnabled = false;
                    if (scenarioState.temperatureIncreaseC <= 0.0f) scenarioState.temperatureIncreaseC = 2.0f;
                    result.scenarioStateChanged = true;
                }
                ImGui::SameLine(0, 10.0f);
                if (ImGui::RadioButton("Flood", scenarioState.floodEnabled)) {
                    scenarioState.floodEnabled = true;
                    scenarioState.heatwaveEnabled = false;
                    scenarioState.interventionEnabled = false;
                    scenarioState.rainfallScenarioMm = 80.0f;
                    result.scenarioStateChanged = true;
                }
                ImGui::SameLine(0, 10.0f);
                if (ImGui::RadioButton("What-If Intervention", scenarioState.interventionEnabled)) {
                    scenarioState.interventionEnabled = true;
                    scenarioState.heatwaveEnabled = false;
                    scenarioState.floodEnabled = false;
                    if (scenarioState.vegetationDeltaPct <= 0.0f && scenarioState.shadeDeltaPct <= 0.0f) {
                        scenarioState.vegetationDeltaPct = 0.15f;
                    }
                    result.scenarioStateChanged = true;
                }

                ImGui::SameLine(std::max(380.0f, bottomWidth - 150.0f));
                ImGui::TextDisabled("MODELLED ESTIMATE");
                ImGui::Separator();

                // --- HEATWAVE SCENARIO CONTROLS ---
                if (scenarioState.heatwaveEnabled) {
                    ImGui::TextUnformatted("Heatwave Offset:");
                    ImGui::SameLine(140.0f);
                    for (int value = 1; value <= 5; ++value) {
                        if (value > 1) ImGui::SameLine();
                        char label[8]; std::snprintf(label, sizeof(label), "+%dC", value);
                        if (ImGui::Button(label)) {
                            scenarioState.temperatureIncreaseC = static_cast<float>(value);
                            result.scenarioStateChanged = true;
                        }
                    }
                    ImGui::SameLine(0, 15.0f);
                    float prevTemp = scenarioState.temperatureIncreaseC;
                    ImGui::SetNextItemWidth(180.0f);
                    if (ImGui::SliderFloat("Custom Temp", &scenarioState.temperatureIncreaseC, 0.0f, 10.0f, "+%.1f C")) {
                        result.scenarioStateChanged = true;
                    }
                    ImGui::TextDisabled("Base Temp: %.1f C   Simulated Temp: %.1f C   (Heat risk rescored on all zones)",
                        weather.valid ? weather.currentTemperature : 34.5f,
                        (weather.valid ? weather.currentTemperature : 34.5f) + scenarioState.temperatureIncreaseC);
                }

                // --- FLOOD SCENARIO CONTROLS ---
                if (scenarioState.floodEnabled) {
                    ImGui::TextUnformatted("Rainfall Scenario:");
                    ImGui::SameLine(150.0f);
                    if (ImGui::Button("Heavy Rainfall (80 mm)")) {
                        scenarioState.rainfallScenarioMm = 80.0f;
                        result.scenarioStateChanged = true;
                    }
                    ImGui::SameLine(0, 10.0f);
                    if (ImGui::Button("Extreme Torrential (120 mm)")) {
                        scenarioState.rainfallScenarioMm = 120.0f;
                        result.scenarioStateChanged = true;
                    }
                    ImGui::SameLine(0, 15.0f);
                    ImGui::SetNextItemWidth(180.0f);
                    if (ImGui::SliderFloat("Rainfall Volume", &scenarioState.rainfallScenarioMm, 0.0f, 150.0f, "%.0f mm")) {
                        result.scenarioStateChanged = true;
                    }
                    ImGui::TextDisabled("Rainfall Volume: %.0f mm (OSM surface impermeability proxy)", scenarioState.rainfallScenarioMm);
                }

                // --- WHAT-IF INTERVENTION CONTROLS ---
                if (scenarioState.interventionEnabled) {
                    ImGui::PushID("vegControls");
                    ImGui::TextUnformatted("Vegetation Increase:");
                    ImGui::SameLine(160.0f);
                    const float vegPresets[] = { 0.05f, 0.10f, 0.15f, 0.20f };
                    for (int i = 0; i < 4; ++i) {
                        if (i > 0) ImGui::SameLine();
                        char label[16]; std::snprintf(label, sizeof(label), "+%d%%", static_cast<int>(vegPresets[i] * 100.0f));
                        if (ImGui::Button(label)) { scenarioState.vegetationDeltaPct = vegPresets[i]; result.scenarioStateChanged = true; }
                    }
                    ImGui::PopID();

                    ImGui::PushID("shadeControls");
                    ImGui::TextUnformatted("Shade Increase:");
                    ImGui::SameLine(160.0f);
                    const float shadePresets[] = { 0.05f, 0.10f, 0.20f };
                    for (int i = 0; i < 3; ++i) {
                        if (i > 0) ImGui::SameLine();
                        char label[16]; std::snprintf(label, sizeof(label), "+%d%%", static_cast<int>(shadePresets[i] * 100.0f));
                        if (ImGui::Button(label)) { scenarioState.shadeDeltaPct = shadePresets[i]; result.scenarioStateChanged = true; }
                    }
                    ImGui::PopID();

                    ImGui::Spacing();
                    if (selectedZone) {
                        if (ImGui::Checkbox("Apply to Selected Zone Only", &scenarioState.applyToSelectedZoneOnly)) {
                            result.scenarioStateChanged = true;
                        }
                    } else {
                        ImGui::TextDisabled("Apply Scope: City-Wide (Select a zone to target individually)");
                    }
                }
            } else {
                // PHASE 24: EXECUTIVE CHARTS & DATA VISUALIZATION
                ImGui::Columns(4, "bottomMetricsColumns", true);

                // Column 1: Legend
                ImGui::TextColored(kTextMuted, "HEAT RISK LEGEND");
                ImGui::TextColored(kAccentCyan, "0-30   LOW");
                ImGui::TextColored(kAccentGreen, "31-50  MODERATE");
                ImGui::TextColored(kAccentYellow, "51-70  HIGH");
                ImGui::TextColored(kAccentOrange, "71-85  VERY HIGH");
                ImGui::TextColored(kAccentRed, "86-100 EXTREME");
                ImGui::NextColumn();

                // Column 2: Stacked Risk Distribution Bar Chart
                ImGui::TextColored(kTextMuted, "RISK DISTRIBUTION");
                int cLow = 0, cMod = 0, cHigh = 0, cVHigh = 0, cExt = 0;
                for (const auto& z : zones) {
                    if (z.heatRisk <= 30.0f) ++cLow;
                    else if (z.heatRisk <= 50.0f) ++cMod;
                    else if (z.heatRisk <= 70.0f) ++cHigh;
                    else if (z.heatRisk <= 85.0f) ++cVHigh;
                    else ++cExt;
                }
                int totalZ = static_cast<int>(zones.size());
                float tF = totalZ > 0 ? static_cast<float>(totalZ) : 1.0f;

                // Stacked horizontal bar chart
                float barWidth = 140.0f, barHeight = 14.0f;
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 p = ImGui::GetCursorScreenPos();
                float xOffset = p.x;

                const struct { float count; ImVec4 col; } segments[] = {
                    { cLow / tF, kAccentCyan },
                    { cMod / tF, kAccentGreen },
                    { cHigh / tF, kAccentYellow },
                    { cVHigh / tF, kAccentOrange },
                    { cExt / tF, kAccentRed }
                };

                for (const auto& seg : segments) {
                    float segW = seg.count * barWidth;
                    if (segW > 0.5f) {
                        dl->AddRectFilled(ImVec2(xOffset, p.y), ImVec2(xOffset + segW, p.y + barHeight), ImGui::ColorConvertFloat4ToU32(seg.col));
                        xOffset += segW;
                    }
                }
                dl->AddRect(p, ImVec2(p.x + barWidth, p.y + barHeight), IM_COL32(80, 100, 120, 180), 3.0f);
                ImGui::Dummy(ImVec2(barWidth, barHeight + 4.0f));

                ImGui::Text("Low: %.0f%%  Mod: %.0f%%", (cLow / tF) * 100.0f, (cMod / tF) * 100.0f);
                ImGui::TextColored(kAccentRed, "High/Extreme: %.0f%%", ((cHigh + cVHigh + cExt) / tF) * 100.0f);
                ImGui::NextColumn();

                // Column 3: Exposed Population Breakdown & Sparkline
                ImGui::TextColored(kTextMuted, "EXPOSED POPULATION");
                int totalExposed = 0, highRiskExposed = 0;
                float popData[16] = { 0 };
                int idx = 0;
                for (const auto& z : zones) {
                    if (!z.populationExposureIsPlaceholder) {
                        totalExposed += z.heatExposedPopulation;
                        if (z.heatRisk >= 70.0f) highRiskExposed += z.population;
                    }
                    if (idx < 16) popData[idx++] = static_cast<float>(z.heatExposedPopulation);
                }

                ImGui::TextColored(kAccentYellow, "%d residents", totalExposed);
                ImGui::SetNextItemWidth(130.0f);
                ImGui::PlotLines("##popSpark", popData, std::min(idx, 16), 0, nullptr, 0.0f, 30000.0f, ImVec2(130, 24));
                ImGui::TextDisabled("High Risk Pop: %d", highRiskExposed);
                ImGui::NextColumn();

                // Column 4: Green Coverage Sparkline & Target Bar
                ImGui::TextColored(kTextMuted, "GREEN INFRASTRUCTURE");
                float sumCoverage = 0.0f;
                float greenData[16] = { 0 };
                int gIdx = 0;
                for (const auto& z : zones) {
                    sumCoverage += z.greenCoverage;
                    if (gIdx < 16) greenData[gIdx++] = z.greenCoverage * 100.0f;
                }
                float meanCov = zones.empty() ? 0.0f : (sumCoverage / zones.size());

                ImGui::Text("Avg Cover: %.1f%%", meanCov * 100.0f);
                ImGui::SetNextItemWidth(130.0f);
                ImGui::PlotLines("##greenSpark", greenData, std::min(gIdx, 16), 0, nullptr, 0.0f, 40.0f, ImVec2(130, 24));
                DrawProgressBar(meanCov / 0.20f, 130.0f, 6.0f, kAccentGreen);
                ImGui::TextDisabled("Target: 20.0%% (Deficit: %.1f%%)", std::max(0.0f, 20.0f - meanCov * 100.0f));
                ImGui::Columns(1);
            }
        }
        ImGui::End();

        // -----------------------------------------------------------------
        // 5. FOOTER STATUS BAR
        // -----------------------------------------------------------------
        ImGui::SetNextWindowPos(ImVec2(0.0f, static_cast<float>(displayHeight) - footerHeight));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(displayWidth), footerHeight));
        if (ImGui::Begin("##FooterBar", nullptr, dockedFlags)) {
            ImGui::TextDisabled("DATA SOURCES: Open-Meteo | OpenStreetMap | WorldPop | Copernicus");
            float rightX = std::max(400.0f, static_cast<float>(displayWidth) - 310.0f);
            ImGui::SameLine(rightX);
            ImGui::TextDisabled("Last Updated: Real-time C++ Digital Twin Engine");
        }
        ImGui::End();

        // Overlay: BEFORE / AFTER labels when comparative mode is active
        if (scenarioState.beforeAfterEnabled) {
            // Small label on the left (BEFORE)
            ImGuiWindowFlags overlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

            ImGui::SetNextWindowPos(ImVec2(12.0f, topBarHeight + 8.0f), ImGuiCond_Always);
            if (ImGui::Begin("##BeforeLabel", nullptr, overlayFlags)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
                ImGui::Text("BEFORE");
                ImGui::PopStyleColor();
            }
            ImGui::End();

            // Small label on the right (AFTER)
            ImGui::SetNextWindowPos(ImVec2(static_cast<float>(displayWidth) - 92.0f, topBarHeight + 8.0f), ImGuiCond_Always);
            if (ImGui::Begin("##AfterLabel", nullptr, overlayFlags)) {
                ImGui::PushStyleColor(ImGuiCol_Text, kAccentGreen);
                ImGui::Text("AFTER");
                ImGui::PopStyleColor();
            }
            ImGui::End();
        }

        return result;
    }

}  // namespace twin
