#pragma once
#include "../core/Mesh.h"
#include "../gis/GISTypes.h"

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
};

// Pure geometry generation: polygon -> extruded walls/roof, polyline ->
// ribbon, polygon -> flat cap, point -> marker. No GL state changes besides
// the final Mesh::Upload() calls, so this is easy to reason about/test
// independent of the render loop.
class CityMeshBuilder {
public:
    static CityMeshes Build(const gis::GISDataset& dataset);
};

}  // namespace twin
