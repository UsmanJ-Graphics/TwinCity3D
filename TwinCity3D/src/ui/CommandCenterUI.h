#pragma once

#include "../twin/Zone.h"
#include "../twin/HeatLayers.h"
#include "../twin/WeatherData.h"
#include "../twin/PopulationData.h"
#include "../ui/ScenarioPanel.h"
#include "../core/Camera.h"
#include "../twin/PriorityModel.h"
#include "../twin/CitizenReport.h"
#include <vector>
#include <optional>

namespace twin {

    // Action structure returned by CommandCenterUI each frame so Application
    // can handle scene updates, camera snaps, and mesh rebuilds cleanly.
    struct UICommandResult {
        std::optional<DataLayer> newActiveLayer;
        std::optional<CameraMode> newCameraMode;
        int focusedZoneId{ -1 };
        bool scenarioStateChanged{ false };
        // Request that Application save baseline metrics (before applying interventions)
        bool saveBaselineRequested{ false };
        // New citizen report created via UI
        std::optional<CitizenReport> newCitizenReport;
        // Request to select a report (id), returned when user clicks a report in list
        int requestedSelectedReportId{ -1 };
        // Request to update a report's status: id and new status value (static_cast<int>(ReportStatus))
        int reportStatusUpdateId{ -1 };
        int reportStatusUpdateValue{ -1 };

        bool resetCameraRequested{ false };
    };

    class CommandCenterUI {
    public:
        // Configures custom Dark Command-Center ImGui visual style.
        static void InitStyle();

        // Renders the unified full-screen Command Center interface:
        // - Top Bar (Title, live status, weather readout, camera controls)
        // - Left Navigation Sidebar (Category tabs & layer switches)
        // - Right Inspector Panel (Zone Intelligence & Top Priority list)
        // - Bottom Analytics Dashboard (Legend, Risk Distribution, Metrics & Scenario Controls)
        // - Bottom Status Bar (Data sources & timestamp)
        static UICommandResult Render(
            DataLayer activeLayer,
            CameraMode cameraMode,
            const Zone* selectedZone,
            const std::vector<Zone>& zones,
            const std::vector<CitizenReport>& reports,
            int selectedReportId,
            ScenarioState& scenarioState,
            const WeatherData& weather,
            const PopulationData& population,
            int displayWidth,
            int displayHeight
        );
    };

}  // namespace twin
