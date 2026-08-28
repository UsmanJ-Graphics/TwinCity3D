#pragma once
#include "../twin/Zone.h"
#include <vector>

namespace twin {

    // Phase 19: renders the "GREEN INFRASTRUCTURE" panel.
    //
    // The panel answers three questions the master spec identifies as the
    // core purpose of the green layer:
    //
    //   WHERE should we add greenery?     (top green-priority zones, ranked)
    //   WHERE is the green deficit?       (per-zone deficit bar, heatmap legend)
    //   WHAT IS the green coverage?       (current vs target coverage)
    //
    // This is strictly a presentation class — it never mutates zone data
    // or drives the camera. Like Inspector/LayersPanel, it returns an
    // action (clicked zone id) to Application, which decides what "click"
    // means (select + focus camera).
    //
    // Every displayed score carries "Prototype modelled estimate" text, per
    // the master spec's data-credibility requirement (Phase 36).
    class GreenInfrastructurePanel {
    public:
        // Draws the panel.
        //   zones       — full zone list from DigitalTwin::Zones().
        //   maxEntries  — how many top-priority zones to list.
        //
        // Returns the zone id the user clicked this frame (-1 if nothing
        // clicked) — same contract as Inspector::RenderTopPriorityPanel.
        static int Render(const std::vector<Zone>& zones, int maxEntries = 5);
    };

}  // namespace twin
