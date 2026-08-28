#pragma once
#include "Zone.h"
#include <string>

namespace twin {

    // Phase 9. The set of per-zone quantities the 3D view can be color-coded
    // by. There is exactly one color ramp (see basic.frag's DataRamp) and
    // one normalization function below — adding a new layer never touches
    // rendering code, matching the master spec's "do not hard-code a
    // particular color palette" rule for Phase 9.
    enum class DataLayer {
        HeatRisk,
        Population,
        GreenCoverage,
        BuildingDensity,
        Temperature,
        FloodRisk
    };

    // Console-legend text for the active layer (there's no ImGui panel yet —
    // this plays the same "temporary diagnostic, superseded by real UI
    // later" role as Application's existing LogPhase*Summary() methods).
    struct LayerInfo {
        std::string name;
        std::string units;
        std::string lowLabel;
        std::string highLabel;
    };

    // Written into a vertex's dataValue when its zone doesn't have real data
    // for the selected layer yet (still a placeholder). Real values are
    // always clamped to [0,1], so this sentinel is unambiguous — the shader
    // renders it as neutral gray rather than a fabricated color (Critical
    // Engineering Rule #2: never present placeholder data as real).
    constexpr float kNoDataSentinel = -1.0f;

    DataLayer NextDataLayer(DataLayer current);
    LayerInfo DescribeLayer(DataLayer layer);

    // Normalizes `zone`'s value for `layer` to [0,1], or returns
    // kNoDataSentinel if that zone has no real data for this layer yet.
    //
    // `minPopDensity`/`maxPopDensity` are the study-area min/max population
    // density among zones with real (non-placeholder) population. Unlike
    // heat risk (fixed 0-100) or temperature (fixed HeatRiskModel scale),
    // population density has no universal ceiling — same reasoning as
    // HeatRiskModel::Compute()'s populationExposureScore — so the caller
    // computes these once across all zones and passes them in rather than
    // this function re-deriving them per zone.
    float NormalizedLayerValue(const Zone& zone, DataLayer layer,
        float minPopDensity, float maxPopDensity);

}  // namespace twin
