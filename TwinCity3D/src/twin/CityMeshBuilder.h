#pragma once
#include "../core/Mesh.h"
#include "../gis/GISTypes.h"
#include "Zone.h"
#include "HeatLayers.h"

namespace twin {

// One merged, ready-to-draw mesh per data category. Merging every building
// (etc.) into a single VAO keeps Phase 3 within the Phase 19 performance
// budget (no per-building draw calls) without needing real batching
// infrastructure yet — see Renderer.h's note on where that hooks in later.
struct CityMeshes {
    Mesh buildings;
    Mesh roads;
    Mesh greenAreas;
    Mesh facilities;

    int buildingCount{0};
    int roadCount{0};
    int greenAreaCount{0};
    int facilityCount{0};
    int skippedBuildingCount{0};  // footprints that failed to triangulate

    // Phase 9: which layer buildings/greenAreas are currently colored by.
    // Roads/facilities always render their flat category color regardless
    // of this — they don't belong to a single zone the way buildings and
    // green areas do.
    DataLayer activeLayer{DataLayer::HeatRisk};
};

// Pure geometry generation: polygon -> extruded walls/roof, polyline ->
// ribbon, polygon -> flat cap, point -> marker. No GL state changes besides
// the final Mesh::Upload() calls, so this is easy to reason about/test
// independent of the render loop.
class CityMeshBuilder {
public:
    // Phase 3: category-only build, no zone data yet. Used for the very
    // first LoadCityData() call in Application::Init(), which runs before
    // DigitalTwin::Build() has produced any zones. Equivalent to
    // Build(dataset, {}, DataLayer::HeatRisk) — every building/green area
    // comes back gray (Phase 9 "no data yet" sentinel) until the caller
    // rebuilds with real zone data.
    static CityMeshes Build(const gis::GISDataset& dataset);

    // Phase 9: builds the same geometry, but bakes each building's/green
    // area's zone value for `layer` into every vertex's dataValue channel
    // (consumed by basic.frag's DataRamp) instead of leaving it at the
    // "no data" sentinel. A building/green area whose zoneId has no match
    // in `zones` also gets the sentinel, same as a placeholder zone.
    //
    // This is a full geometry rebuild (new VBOs), not a cheap per-vertex
    // update, so callers should only invoke it on a user action (loading
    // real data, switching the active layer, a Phase 12 scenario recompute)
    // — never per frame (Phase 19).
    static CityMeshes Build(const gis::GISDataset& dataset,
                             const std::vector<Zone>& zones,
                             DataLayer layer);
};

}  // namespace twin
