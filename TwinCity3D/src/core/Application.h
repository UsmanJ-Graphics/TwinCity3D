#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

#include "Camera.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include "../gis/GISTypes.h"
#include "../twin/CityMeshBuilder.h"
#include "../twin/DigitalTwin.h"

namespace twin {

    // Owns window lifecycle, the render loop, and top-level input handling.
    // Phase 0 gave it a window + working FPS camera over a ground grid. Phase 3
    // adds the 3D city model: buildings, roads, green areas and facilities are
    // loaded once at Init() from data/processed/*.json and rendered as four
    // merged meshes, colored per-category (a real data-driven heat-risk palette
    // arrives in Phase 9 — Mesh::dataValue already carries a per-vertex channel
    // for that). Zone inspection / scenario / UI panels attach in later phases.
    class Application {
    public:
        Application(int width, int height, const std::string& title)
            : m_width(width), m_height(height), m_title(title) {
        }

        ~Application() { Shutdown(); }

        bool Init();
        void Run();
        void Shutdown();

    private:
        void ProcessInput(float dt);
        void LoadCityData();
        void LogPhase4ZoneSummary();  // temporary Phase 4 diagnostic; superseded by the Zone Inspector UI (Phase 10)

        static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

        GLFWwindow* m_window{ nullptr };
        int m_width;
        int m_height;
        std::string m_title;

        Camera m_camera;
        Renderer m_renderer;
        Shader m_gridShader;
        Mesh m_groundGrid;

        gis::GISDataset m_gisDataset;
        CityMeshes m_city;
        DigitalTwin m_digitalTwin;
        bool m_hasCityData{ false };

        bool m_firstMouse{ true };
        float m_lastMouseX{ 0.0f };
        float m_lastMouseY{ 0.0f };
        bool m_mouseLookEnabled{ true };
    };

}  // namespace twin