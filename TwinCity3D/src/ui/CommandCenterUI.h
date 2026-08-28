#pragma once

#include "../twin/Zone.h"
#include "../twin/HeatLayers.h"
#include "../twin/WeatherData.h"
#include "../twin/PopulationData.h"
#include "../ui/ScenarioPanel.h"
#include "../core/Camera.h"
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
            ScenarioState& scenarioState,
            const WeatherData& weather,
            const PopulationData& population,
            int displayWidth,
            int displayHeight
        );
    };

}  // namespace twin
