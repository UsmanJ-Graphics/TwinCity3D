#pragma once

#include "../twin/Zone.h"
#include <vector>

namespace twin {

    struct ScenarioState {
        bool heatwaveEnabled{ false };
        float temperatureIncreaseC{ 0.0f };
    };

    class ScenarioPanel {
    public:
        // Returns true when a control changed and Application should rerun the
        // scenario pipeline. The panel never mutates zone scores itself.
        static bool Render(ScenarioState& state, const std::vector<Zone>& zones,
            float baselineTemperatureC);
    };

}  // namespace twin
