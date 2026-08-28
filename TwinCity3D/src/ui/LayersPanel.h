#pragma once
#include "../twin/HeatLayers.h"
#include <optional>

namespace twin {

    // Phase 13: renders the "CITY LAYERS" panel from the master spec's
    // command-center mock-up — an on-screen alternative to the Phase 9
    // keyboard shortcuts (1-5, L) for picking which single DataLayer
    // currently colors the 3D twin's buildings/green areas. Both the
    // keyboard and this panel read/write the same Application::m_activeLayer,
    // so they always stay in sync with each other.
    //
    // Per the master spec's Phase 13 instruction — "do NOT create ten
    // completely different maps... use the same spatial digital twin and
    // modify the visualization layer" — exactly one layer is active at a
    // time. This is a single-select list (clicking a row selects it and
    // implicitly deselects the rest), not independent toggle checkboxes,
    // even though the spec's Phase 22 mock-up draws checkbox glyphs (only
    // one of which is ever checked in that mock-up).
    //
    // Only lists layers that have real per-zone data behind them today (see
    // HeatLayers.h's DataLayer enum / CityMeshBuilder). The full Phase 13
    // wishlist also includes Satellite / Flood Risk / Air Quality / Light
    // Pollution / Population Density — those need their own data pipelines
    // (Phase 18/25/26) and are deliberately left out rather than added as
    // dead entries that would always render "no data" gray (Critical
    // Engineering Rule #2: never present a placeholder as real).
    class LayersPanel {
    public:
        // Draws the panel and returns the layer the user clicked this
        // frame, or std::nullopt if nothing was clicked — mirrors
        // Inspector::RenderTopPriorityPanel's "pure presentation, caller
        // decides what a click means" contract. Never mutates
        // `activeLayer`; Application::SetActiveLayer() is the one place
        // that owns the switch (and the mesh rebuild it triggers).
        static std::optional<DataLayer> Render(DataLayer activeLayer);
    };

}  // namespace twin
