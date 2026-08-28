#pragma once
#include "../core/Mesh.h"
#include "Zone.h"

namespace twin {

    // Phase 14: builds a flat rectangular border-frame mesh outlining a
    // zone's XZ bounds (Zone::minX/maxX/minZ/maxZ), sitting at a small
    // ground elevation. Before this, the only sign a zone was selected was
    // text in the Inspector panel — nothing in the 3D view itself showed
    // WHERE the selected zone actually is, which matters for a
    // decision-support twin (see master spec's "selected-zone highlighting"
    // under Phase 14, and the WHERE/WHO/WHY/WHAT-IF workflow it's built
    // around).
    //
    // Pure geometry generation only, same contract as CityMeshBuilder: no
    // GL state besides the final Mesh::Upload(). Color and any pulse
    // animation are a draw-call concern (see Application::Run()) — this
    // class never touches a shader.
    class SelectionHighlight {
    public:
        // `thickness` is the frame's width in meters; internally clamped so
        // it can never exceed roughly a third of the zone's smaller
        // dimension, so a small zone still gets a visible ring instead of
        // an inverted/degenerate one. `y` lifts the frame just above the
        // road mesh (0.05) so it isn't z-fighted out on open ground.
        static Mesh Build(const Zone& zone, float thickness = 2.0f, float y = 0.08f);
    };

}  // namespace twin
