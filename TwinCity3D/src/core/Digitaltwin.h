 #pragma once
#include <string>

#include "GISLoader.h"
#include "Mesh.h"
#include "Shader.h"

namespace twin {

    // Phase 3 façade: loads the Phase 2 processed GIS data and builds the four
    // merged meshes (buildings, roads, green areas, facilities) once at startup.
    // Application only needs to call Load() in Init() and Draw() each frame --
    // it doesn't need to know about GISLoader/MeshBuilder directly.
    //
    // Layer colors are passed into Draw() rather than hard-coded here, per
    // Phase 4's rule ("do not hard-code visual colors ... into rendering code")
    // -- this class draws whatever colors it's told, so Phase 9's heat-risk
    // layer can later override them without touching this file.
    class DigitalTwin {
    public:
        bool Load(const std::string& processedDir);

        struct LayerColors {
            glm::vec4 buildings{ 0.62f, 0.62f, 0.66f, 1.0f };
            glm::vec4 roads{ 0.22f, 0.22f, 0.24f, 1.0f };
            glm::vec4 greenAreas{ 0.22f, 0.62f, 0.28f, 1.0f };
            glm::vec4 facilities{ 0.90f, 0.55f, 0.10f, 1.0f };
        };

        // Draws all four layers using the given shader (already bound with
        // uModel/uView/uProjection set by the caller). Sets uBaseColor per
        // layer before each draw call.
        void Draw(const Shader& shader) const { Draw(shader, LayerColors{}); }
        void Draw(const Shader& shader, const LayerColors& colors) const;

        const GISLoader& Loader() const { return m_loader; }
        bool UsingSampleData() const { return m_loader.UsingSampleData(); }

    private:
        GISLoader m_loader;
        Mesh m_buildingsMesh;
        Mesh m_roadsMesh;
        Mesh m_greenAreasMesh;
        Mesh m_facilitiesMesh;
        bool m_loaded{ false };
    };

}  // namespace twin