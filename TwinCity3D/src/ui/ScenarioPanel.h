#pragma once

#include "../twin/Zone.h"
#include <vector>

namespace twin {

    struct ScenarioState {
        bool heatwaveEnabled{ false };
        float temperatureIncreaseC{ 0.0f };
        bool floodEnabled{ false };
        float rainfallScenarioMm{ 80.0f };

        // Phase 20: What-If Intervention Engine
        bool interventionEnabled{ false };
        float vegetationDeltaPct{ 0.0f }; // e.g. 0.05, 0.10, 0.15, 0.20 (+5%..+20%)
        float shadeDeltaPct{ 0.0f };      // e.g. 0.05, 0.10, 0.20 (+5%..+20%)
        bool applyToSelectedZoneOnly{ false };
        // Phase 21: Before / After comparative mode
        bool beforeAfterEnabled{ false };
        // 0 = side-by-side, 1 = slider wipe
        int beforeAfterMode{ 0 };
        // slider position 0.0 (show baseline) .. 1.0 (show scenario)
        float beforeAfterSlider{ 0.5f };
    };

    class ScenarioPanel {
    public:
        // Returns true when a control changed and Application should rerun the
        // scenario pipeline. The panel never mutates zone scores itself.
        static bool Render(ScenarioState& state, const std::vector<Zone>& zones,
            float baselineTemperatureC, float baselineRainfallMm, int selectedZoneId = -1);
    };

}  // namespace twin
