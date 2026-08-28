#pragma once
#include "../twin/HeatLayers.h"

namespace twin {

    // Phase 14: on-screen legend for whichever DataLayer currently colors
    // the 3D twin. Before this, the only legend was a console log line
    // (Application::LogActiveLayerLegend) — useful for the developer at a
    // terminal, useless for a planner looking at the running application.
    //
    // The gradient this panel draws is hardcoded to the exact same 5 stop
    // colors as basic.frag's DataRamp(). That duplication is deliberate,
    // not an oversight: a GLSL shader and C++ ImGui draw call can't share
    // one literal color table without a much bigger refactor (e.g. a
    // shared palette resource both sides load), which is out of scope for
    // an MVP legend. If DataRamp's stops ever change, kLegendStops below
    // must change with them, or the legend will silently drift out of
    // sync with what's actually on screen — flagged here so that's not a
    // surprise later.
    class HeatLegend {
    public:
        // For HeatRisk specifically, band labels match
        // HeatRiskModel::Classify's actual output strings (Zone::riskClass:
        // "Low"/"Moderate"/"High"/"Very High"/"Extreme") rather than the
        // master spec mock-up's "COOL...EXTREME" wording, so the legend
        // never uses a term the app itself doesn't use elsewhere (e.g. the
        // Inspector's "HEAT RISK ... (High)" line). Every other layer has
        // no discrete classification model behind it yet — just a
        // continuous 0..1 normalized value (see NormalizedLayerValue) — so
        // those show the same 5-color gradient under HeatLayers::
        // DescribeLayer()'s existing low/high labels instead of 5 band
        // names.
        static void Render(DataLayer activeLayer);
    };

}  // namespace twin
