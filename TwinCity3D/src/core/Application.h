#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

#include "Camera.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"
#include "Picking.h"

#include "../gis/GISTypes.h"
#include "../twin/CityMeshBuilder.h"
#include "../twin/DigitalTwin.h"
#include "../twin/WeatherData.h"
#include "../twin/PopulationData.h"
#include "../twin/HeatLayers.h"
#include "../ui/Inspector.h"

namespace twin {

    // Owns window lifecycle, the render loop, and top-level input handling.
    // Phase 0 gave it a window + working FPS camera over a ground grid. Phase 3
    // adds the 3D city model: buildings, roads, green areas and facilities are
    // loaded once at Init() from data/processed/*.json and rendered as four
    // merged meshes. Phase 9 replaces the flat per-category building/green-area
    // color with a data-driven ramp: m_activeLayer selects which zone quantity
    // drives that color, number keys 1-5 (+ L to cycle) switch it at runtime,
    // and RebuildCityMeshForActiveLayer() re-bakes CityMeshes accordingly.
    // Phase 10 adds zone selection: a left mouse click is unprojected into a
    // ground-plane ray (Picking), resolved to a zone id, and drawn via the
    // ImGui-based Inspector panel every frame. Scenario/decision-dashboard
    // panels attach in later phases.
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

        void LoadWeather();            // Phase 5, Step 3: loads weather.json, applies it to the digital twin
        void LogPhase5WeatherSummary(); // temporary Phase 5 diagnostic; superseded by the dashboard's current-temperature display (Phase 15)

        void LoadPopulation();            // Phase 7: loads population.json, applies it to the digital twin
        void LogPhase7PopulationSummary(); // temporary Phase 7 diagnostic; superseded by the dashboard's population-exposure display (Phase 15)
        void ComputeHeatRisk();
        void LogPhase8HeatRiskSummary();

        // Phase 9: rebuilds m_city with m_activeLayer's data baked into
        // buildings'/green areas' vertex colors, and logs the console
        // legend. Called once after ComputeHeatRisk() in Init() (so the
        // very first frame already shows real data, not the Phase 3
        // placeholder gray) and again whenever SetActiveLayer() runs. A
        // full geometry rebuild — only ever called on a user action, never
        // per frame (Phase 19).
        void RebuildCityMeshForActiveLayer();
        void SetActiveLayer(DataLayer layer);
        void LogActiveLayerLegend();

        // Phase 10: initializes/tears down the Dear ImGui context (GLFW +
        // OpenGL3 backends) used by Inspector and later UI panels. Kept
        // separate from Init()/Shutdown() bodies so it's obvious where the
        // ImGui lifecycle lives if a later phase needs to add more panels
        // that share the same context.
        bool InitImGui();
        void ShutdownImGui();

        // Phase 10: called from the mouse-button callback on a left click.
        // The camera runs with GLFW_CURSOR_DISABLED for FPS-style mouse-look,
        // so glfwGetCursorPos() reports an unbounded virtual delta, not a
        // real on-screen pixel — there's no meaningful "where the user
        // clicked" position to unproject. Instead this picks whatever's at
        // screen-center (a crosshair), matching how free-look FPS cameras
        // conventionally handle picking: aiming *is* the camera direction.
        // Updates m_selectedZoneId (-1 if the center ray missed every zone,
        // e.g. pointed at the sky). Ignores the click while an ImGui window
        // wants mouse focus, so interacting with the Inspector panel itself
        // doesn't reselect/deselect through it.
        void HandleZonePick();

        static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
        static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);  // Phase 10

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

        WeatherData m_weather;  // Phase 5, Step 3
        PopulationData m_population;  // Phase 7

        DataLayer m_activeLayer{ DataLayer::HeatRisk };  // Phase 9

        int m_selectedZoneId{ -1 };  // Phase 10: -1 means nothing selected

        bool m_firstMouse{ true };
        float m_lastMouseX{ 0.0f };
        float m_lastMouseY{ 0.0f };
        bool m_mouseLookEnabled{ true };
    };

}  // namespace twin