#pragma once
#include "../twin/Zone.h"

namespace twin {

    // Phase 10: renders the "ZONE INFORMATION" panel from the master spec's
    // mock-up (temperature / population / green coverage / building density
    // / heat risk / priority) for whichever zone is currently selected.
    //
    // This is pure presentation over data that already exists on Zone by the
    // time Phase 10 runs (Phase 3/6/7/8) — the Inspector never computes or
    // mutates anything, it only formats. Selection itself is owned by
    // Application (via Picking), not by this class, so Inspector stays a
    // stateless "given a zone, draw it" panel that later phases (11 priority,
    // 14 before/after) can reuse without restructuring.
    //
    // Uses Dear ImGui, per the master spec's Phase 0 note that ImGui is the
    // intended UI toolkit for the /ui panels. Requires ImGui's GLFW + OpenGL3
    // backends to already be initialized and a NewFrame()/Render() pair
    // wrapped around the call site (see Application::Run()).
    class Inspector {
    public:
        // Draws the panel. `zone` may be nullptr (nothing selected yet), in
        // which case a short "click a zone to inspect it" placeholder is
        // shown instead of empty/garbage fields.
        static void Render(const Zone* zone);

    private:
        // Renders the populated version of the panel for a non-null zone,
        // including a "PLACEHOLDER" tag on any field whose data hasn't
        // landed yet (temperature/population before Phase 5/7 ran, or
        // heat risk before Phase 8 has real inputs to score) — the Inspector
        // must never present a placeholder value as if it were real data
        // (Critical Engineering Rule #2/#3).
        static void RenderZoneDetails(const Zone& zone);
    };

}  // namespace twin
