#include "Application.h"
#include "Log.h"

#include "../gis/GISLoader.h"
#include "../twin/WeatherLoader.h"
#include "../twin/PopulationLoader.h"
#include "../twin/SatelliteEnvironmentLoader.h"
#include <iamgui/imgui.h>
#include <iamgui/imgui_impl_glfw.h>
#include <iamgui/imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>  // std::fabs, used by Phase 12's drag-vs-click threshold
#include <optional>  // Phase 13: LayersPanel::Render()'s return type

namespace twin {

    static bool s_keys[GLFW_KEY_LAST + 1] = { false };

    bool Application::Init() {
        if (!glfwInit()) {
            LogError("GLFW initialization failed");
            return false;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

        m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
        if (!m_window) {
            LogError("Failed to create GLFW window");
            glfwTerminate();
            return false;
        }

        glfwMakeContextCurrent(m_window);
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
        glfwSetCursorPosCallback(m_window, CursorPosCallback);
        glfwSetScrollCallback(m_window, ScrollCallback);
        glfwSetKeyCallback(m_window, KeyCallback);
        glfwSetMouseButtonCallback(m_window, MouseButtonCallback);  // Phase 10: zone picking
        glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        glewExperimental = GL_TRUE;
        GLenum glewStatus = glewInit();
        if (glewStatus != GLEW_OK) {
            LogError(std::string("GLEW initialization failed: ") +
                reinterpret_cast<const char*>(glewGetErrorString(glewStatus)));
            return false;
        }

        LogInfo(std::string("OpenGL context: ") + reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        LogInfo(std::string("Renderer: ") + reinterpret_cast<const char*>(glGetString(GL_RENDERER)));

        if (!InitImGui()) {
            LogError("Failed to initialize ImGui (Phase 10 Zone Inspector)");
            return false;
        }

        m_renderer.Init();

        if (!m_gridShader.LoadFromFiles("shaders/basic.vert", "shaders/basic.frag")) {
            LogError("Failed to load Phase 0 grid shader");
            return false;
        }

        m_groundGrid = MakeGroundGrid(900.0f, 60);

        LoadCityData();
        LoadWeather();
        LoadPopulation();
        LoadSatelliteEnvironment();
        m_digitalTwin.ComputeFloodRisk(m_weather.valid ? m_weather.precipitation : 0.0f);
        ComputeHeatRisk();
        ComputePriority();  // Phase 11: builds on ComputeHeatRisk(), must run after it

        // Phase 9: everything the color needs (heat risk, population,
        // green coverage, building density, temperature) is on the zones
        // now, so recolor the city mesh with real data before the first
        // frame renders instead of leaving it at LoadCityData()'s
        // placeholder gray.
        RebuildCityMeshForActiveLayer();

        LogInfo("Phase 0-11 initialization complete: window + camera + 3D city model + weather + "
            "population + heat risk + priority ranking + data-driven layer visualization ready");
        LogInfo("Phase 12 camera controls: [F] Free-fly  [O] Orbit  [T] Top-down  [I] Isometric  "
            "-- Orbit: left-drag rotate, right-drag pan, scroll dolly. TopDown/Isometric: drag to pan. "
            "[R] hard reset to Free-fly.");
        return true;
    }

    void Application::LoadCityData() {
        // Relative to the working directory the binary is run from (see
        // README run instructions) — matches CMake's default runtime dir.
        const std::string dataDir = "data/processed";

        if (!gis::GISLoader::Load(dataDir, m_gisDataset)) {
            LogWarn("No GIS data found at '" + dataDir + "'. Run python/build_sample_data.py "
                "(or the full pipeline) to generate it. Falling back to ground grid only.");
            m_hasCityData = false;
            return;
        }

        m_city = CityMeshBuilder::Build(m_gisDataset);
        m_hasCityData = true;

        m_digitalTwin.Build(m_gisDataset);
        LogPhase4ZoneSummary();

        if (m_gisDataset.isSampleData) {
            LogWarn("Loaded dataset is SAMPLE DATA (synthetic fallback), not a live OSM extract. "
                "See docs/data_sources.md.");
        }
    }

    void Application::LogPhase4ZoneSummary() {
        // Console-only diagnostic for Phase 4: proves the zone data model is
        // populated before any UI exists to inspect it (Phase 10 replaces this
        // with an actual on-screen Zone Inspector panel).
        LogInfo("---- Phase 4 zone data model ----");
        for (const auto& z : m_digitalTwin.Zones()) {
            std::string line = "Zone " + std::to_string(z.id) +
                ": buildingDensity=" + std::to_string(z.buildingDensity) +
                " greenCoverage=" + std::to_string(z.greenCoverage) +
                " exposedSurfaceRatio=" + std::to_string(z.exposedSurfaceRatio) +
                " heatBurden=" + std::to_string(z.environmentalHeatBurden) +
                " population=" + (z.populationIsPlaceholder ? "(placeholder)" : std::to_string(z.population)) +
                " populationDensity=" + (z.populationIsPlaceholder ? "(placeholder)" : std::to_string(z.populationDensity) + "/km2") +
                " temperature=" + (z.temperatureIsPlaceholder ? "(placeholder)" : std::to_string(z.temperature)) +
                " heatRisk=" + (z.riskIsPlaceholder ? "(placeholder)" : std::to_string(z.heatRisk));
            LogInfo(line);
        }
        LogInfo("----------------------------------");
    }

    void Application::LoadWeather() {
        // Relative to the working directory, same convention as LoadCityData().
        const std::string weatherPath = "data/processed/weather.json";

        bool ok = WeatherLoader::Load(weatherPath, m_weather);
        if (!ok) {
            LogWarn("No usable weather data at '" + weatherPath + "'. Run "
                "python/fetch_weather.py to generate it. Zone temperatures remain "
                "placeholders until this is fixed.");
        }

        // Always call this, even on failure: ApplyWeather() checks
        // m_weather.valid itself and is a no-op when it's false, so this
        // just keeps the "clear placeholders when data is good" logic in one
        // place (DigitalTwin) rather than duplicating the check here too.
        m_digitalTwin.ApplyWeather(m_weather);
        LogPhase5WeatherSummary();
    }

    void Application::LogPhase5WeatherSummary() {
        // Console-only diagnostic for Phase 5, same role LogPhase4ZoneSummary()
        // plays for Phase 4 — superseded once the dashboard (Phase 15) shows
        // current temperature on screen.
        if (!m_weather.valid) {
            LogWarn("Phase 5 summary: no weather data loaded.");
            return;
        }

        LogInfo("Phase 5 summary: current=" + std::to_string(m_weather.currentTemperature) +
            "C (feels like " + std::to_string(m_weather.apparentTemperature) + "C), " +
            "humidity=" + std::to_string(m_weather.humidity) + "%, " +
            "wind=" + std::to_string(m_weather.windSpeed) + "km/h, " +
            "source=" + m_weather.dataSource +
            (m_weather.IsLive() ? "" : " (NOT live)"));

        int cleared = 0;
        for (const auto& zone : m_digitalTwin.Zones()) {
            if (!zone.temperatureIsPlaceholder) ++cleared;
        }
        LogInfo("Phase 5 summary: temperature placeholder cleared on " +
            std::to_string(cleared) + "/" +
            std::to_string(m_digitalTwin.Zones().size()) + " zones");
    }

    void Application::LoadPopulation() {
        // Relative to the working directory, same convention as LoadCityData()/LoadWeather().
        const std::string populationPath = "data/processed/population.json";

        bool ok = PopulationLoader::Load(populationPath, m_population);
        if (!ok) {
            LogWarn("No usable population data at '" + populationPath + "'. Run "
                "python/estimate_population.py to generate it. Zone population remains "
                "placeholders until this is fixed.");
        }

        // Same pattern as LoadWeather(): always call ApplyPopulation(), even
        // on failure — it checks m_population.valid itself and is a no-op
        // when false, keeping the "clear placeholders when data is good"
        // logic in DigitalTwin rather than duplicating the check here too.
        m_digitalTwin.ApplyPopulation(m_population);
        LogPhase7PopulationSummary();
    }

    void Application::LoadSatelliteEnvironment() {
        const std::string path = "data/processed/satellite_environment.json";
        if (!SatelliteEnvironmentLoader::Load(path, m_satelliteEnvironment)) {
            LogWarn("No processed environmental layer at '" + path + "'. Run python/build_environmental_layer.py.");
            return;
        }
        m_digitalTwin.ApplySatelliteEnvironment(m_satelliteEnvironment);
    }

    void Application::LogPhase7PopulationSummary() {
        // Console-only diagnostic for Phase 7, same role LogPhase5WeatherSummary()
        // plays for Phase 5 — superseded once the dashboard (Phase 15) shows
        // high-risk population exposure on screen.
        if (!m_population.valid) {
            LogWarn("Phase 7 summary: no population data loaded.");
            return;
        }

        LogInfo("Phase 7 summary: source=" + m_population.dataSource +
            (m_population.IsLive() ? "" : " (estimated — not a measured/census figure)") +
            ", method=" + m_population.method +
            ", study-area total population=" + std::to_string(m_population.totalPopulation));

        int cleared = 0;
        for (const auto& zone : m_digitalTwin.Zones()) {
            if (!zone.populationIsPlaceholder) ++cleared;
        }
        LogInfo("Phase 7 summary: population placeholder cleared on " +
            std::to_string(cleared) + "/" +
            std::to_string(m_digitalTwin.Zones().size()) + " zones");
    }

    void Application::ComputeHeatRisk() {
        // Phase 8: no file to load — this recombines fields already present
        // on each zone (temperature from Phase 5/6, buildingDensity/
        // greenCoverage from Phase 3, populationDensity from Phase 7) into an
        // explainable heat-risk score. Uses HeatRiskWeights{} defaults (0.35
        // temperature / 0.20 green deficit / 0.20 building density / 0.25
        // population exposure), matching the master spec's Phase 8 example
        // model. Safe to call again later (e.g. after Phase 12's heatwave
        // scenario adjusts zone.temperature) to rescore under a new scenario.
        m_digitalTwin.ComputeHeatRisk();
        m_digitalTwin.ComputePopulationExposure();
        LogPhase8HeatRiskSummary();
    }

    void Application::LogPhase8HeatRiskSummary() {
        // Console-only diagnostic for Phase 8, same role LogPhase7PopulationSummary()
        // plays for Phase 7 — superseded once the dashboard (Phase 15) and the
        // Zone Inspector (Phase 10) show heat risk on screen.
        LogInfo("---- Phase 8 heat risk ----");
        int cleared = 0;
        for (const auto& zone : m_digitalTwin.Zones()) {
            if (zone.riskIsPlaceholder) continue;
            ++cleared;
            LogInfo("Zone " + std::to_string(zone.id) + ": heatRisk=" +
                std::to_string(zone.heatRisk) + " (" + zone.riskClass + ") exposure=" +
                std::to_string(zone.exposure));
        }
        LogInfo("Phase 8 summary: heat risk computed on " + std::to_string(cleared) + "/" +
            std::to_string(m_digitalTwin.Zones().size()) +
            " zones — PROTOTYPE DECISION-SUPPORT SCORE, not a validated medical/scientific model");
        LogInfo("----------------------------");
    }

    void Application::ComputePriority() {
        // Phase 11: no new external data — recombines heatRisk (Phase 8),
        // environmentalHeatBurden (Phase 6), and population (Phase 7)
        // already present on each zone into an explainable priority score +
        // rank. See DigitalTwin::ComputePriority()/PriorityModel.
        m_digitalTwin.ComputePriority();
        LogPhase11PrioritySummary();
    }

    void Application::ApplyHeatwaveScenario() {
        // ApplyWeather() recreates the current-condition, environmental-burden
        // temperature spread first, preventing slider adjustments from
        // stacking heatwave values onto the previous scenario state.
        m_digitalTwin.ApplyWeather(m_weather);
        if (m_scenario.heatwaveEnabled && m_scenario.temperatureIncreaseC > 0.0f) {
            m_digitalTwin.ApplyTemperatureOffset(m_scenario.temperatureIncreaseC);
        }

        m_digitalTwin.ComputeFloodRisk(m_scenario.floodEnabled
            ? m_scenario.rainfallScenarioMm
            : (m_weather.valid ? m_weather.precipitation : 0.0f));

        ComputeHeatRisk();
        ComputePriority();
        RebuildCityMeshForActiveLayer();
    }

    void Application::LogPhase11PrioritySummary() {
        // Console-only diagnostic for Phase 11, same role LogPhase8HeatRiskSummary()
        // plays for Phase 8 — superseded on-screen by Inspector's PRIORITY
        // section and the Top Priority Zones panel, both added this phase.
        LogInfo("---- Phase 11 priority ranking ----");
        for (const auto& zone : m_digitalTwin.Zones()) {
            if (zone.priorityIsPlaceholder) continue;
            LogInfo("Zone " + std::to_string(zone.id) + ": priority=" +
                std::to_string(zone.priority) + " rank=#" + std::to_string(zone.priorityRank) +
                " (heatRisk=" + std::to_string(zone.heatRisk) +
                ", population=" + std::to_string(zone.population) + ")");
        }
        LogInfo("Phase 11 summary: PROTOTYPE DECISION-SUPPORT PRIORITY, not a validated "
            "government prioritization methodology");
        LogInfo("------------------------------------");
    }

    void Application::FocusCameraOnZone(int zoneId) {
        const Zone* zone = m_digitalTwin.FindZone(zoneId);
        if (!zone) {
            LogWarn("Application::FocusCameraOnZone: zone id=" + std::to_string(zoneId) +
                " not found — ignoring");
            return;
        }

        m_selectedZoneId = zoneId;

        // Ground-level zone center. Camera.FocusOn() applies its own
        // height/distance offset on top of this, so we only need the flat
        // (x, z) target here.
        glm::vec3 center(
            (zone->minX + zone->maxX) * 0.5f,
            0.0f,
            (zone->minZ + zone->maxZ) * 0.5f);

        m_camera.FocusOn(center);
        ApplyCursorModeForCurrentCamera();

        LogInfo("Application::FocusCameraOnZone: focused on zone " + std::to_string(zoneId));
    }

    void Application::SetCameraMode(CameraMode mode) {
        m_camera.SetMode(mode);
        ApplyCursorModeForCurrentCamera();

        static const char* kModeNames[] = { "FreeFly", "Orbit", "TopDown", "Isometric" };
        LogInfo(std::string("Application::SetCameraMode: ") + kModeNames[static_cast<int>(mode)]);
    }

    void Application::ApplyCursorModeForCurrentCamera() {
        // FreeFly with mouse-look on: FPS-style capture (Phase 0 behavior,
        // toggleable with TAB). Every other case (FreeFly with look
        // toggled off, or any of the Orbit/TopDown/Isometric modes): a
        // normal, visible cursor, since those modes are driven by
        // click-and-drag rather than raw unbounded mouse delta, and a
        // visible cursor is also what lets the ImGui panels stay usable.
        bool wantsCapture = (m_camera.GetMode() == CameraMode::FreeFly) && m_mouseLookEnabled;
        glfwSetInputMode(m_window, GLFW_CURSOR, wantsCapture ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        // Avoids a big cursor-delta snap the next time FreeFly capture
        // re-enables (same reasoning as the existing TAB handler below).
        m_firstMouse = true;
    }

    void Application::RebuildCityMeshForActiveLayer() {
        if (!m_hasCityData) return;

        m_city = CityMeshBuilder::Build(m_gisDataset, m_digitalTwin.Zones(), m_activeLayer);
        LogActiveLayerLegend();
    }

    void Application::SetActiveLayer(DataLayer layer) {
        m_activeLayer = layer;
        RebuildCityMeshForActiveLayer();
    }

    void Application::LogActiveLayerLegend() {
        // Console-only "legend" for Phase 9 — same temporary-diagnostic role
        // as LogPhase4/5/7/8*Summary(), superseded by a real on-screen
        // legend once the dashboard/layer-toggle UI exists (Phase 15/16).
        LayerInfo info = DescribeLayer(m_activeLayer);
        LogInfo("---- Active layer: " + info.name + " (" + info.units + ") ----");
        LogInfo("  " + info.lowLabel + "  ->  " + info.highLabel +
            " (blue -> green -> yellow -> orange -> red)");
        LogInfo("  Gray buildings/green areas = no real data for that zone yet (placeholder)");
        LogInfo("  Keys: [1] Heat Risk  [2] Population  [3] Population Exposure  "
            "[4] Green Coverage  [5] Building Density  [6] Temperature  [7] Flood Risk  [8] Environment  [L] cycle");
        LogInfo("-------------------------------------------------");
    }

    bool Application::InitImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        if (!ImGui_ImplGlfw_InitForOpenGL(m_window, true)) return false;
        if (!ImGui_ImplOpenGL3_Init("#version 330")) return false;
        return true;
    }

    void Application::ShutdownImGui() {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void Application::HandleZonePick() {
        if (ImGui::GetIO().WantCaptureMouse) return;

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
        if (fbWidth <= 0 || fbHeight <= 0) return;

        double pickX, pickY;
        bool cursorCaptured = (m_camera.GetMode() == CameraMode::FreeFly) && m_mouseLookEnabled;
        if (cursorCaptured) {
            // Cursor is captured/hidden for FPS-style look — there's no
            // meaningful click position, so pick whatever the camera is
            // actually aimed at (screen-center crosshair).
            pickX = fbWidth / 2.0;
            pickY = fbHeight / 2.0;
        }
        else {
            // Cursor is free (TAB toggled it off, or we're in Orbit/
            // TopDown/Isometric where the cursor is always free) — pick
            // whatever's actually under the visible pointer, like a normal
            // UI click.
            glfwGetCursorPos(m_window, &pickX, &pickY);
        }

        m_selectedZoneId = Picking::PickZoneId(
            pickX, pickY, fbWidth, fbHeight, m_camera, m_digitalTwin.Zones());

        LogInfo("Application::HandleZonePick: selected zone id=" +
            std::to_string(m_selectedZoneId));
    }

    void Application::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (!app) return;

        // FreeFly picks on left press, same as Phase 10 always did — the
        // cursor is captured there, so there's no drag gesture to confuse
        // it with (CursorPosCallback bails out of the Orbit/TopDown/
        // Isometric drag path entirely while in FreeFly).
        if (app->m_camera.GetMode() == CameraMode::FreeFly) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                app->HandleZonePick();
            }
            return;
        }

        // Phase 12: Orbit/TopDown/Isometric use the same left button for
        // both "orbit/pan the camera" and "pick a zone," disambiguated by
        // whether the button moved more than a few pixels before release
        // (see CursorPosCallback's m_didDragThisPress). A right-button
        // press just needs its down/up state tracked for panning; it never
        // picks.
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                app->m_leftMouseDown = true;
                app->m_didDragThisPress = false;
                glfwGetCursorPos(window, &app->m_lastDragX, &app->m_lastDragY);
            } else if (action == GLFW_RELEASE) {
                app->m_leftMouseDown = false;
                if (!app->m_didDragThisPress && !ImGui::GetIO().WantCaptureMouse) {
                    app->HandleZonePick();
                }
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                app->m_rightMouseDown = true;
                glfwGetCursorPos(window, &app->m_lastDragX, &app->m_lastDragY);
            } else if (action == GLFW_RELEASE) {
                app->m_rightMouseDown = false;
            }
        }
    }

    void Application::Run() {
        float lastFrameTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(m_window)) {
            float currentFrameTime = static_cast<float>(glfwGetTime());
            float dt = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;

            glfwPollEvents();
            ProcessInput(dt);
            // Phase 12: advances any in-flight smooth camera transition and
            // keeps m_position synced to orbit state in Orbit/TopDown/
            // Isometric. Camera.h's whole transition/orbit system is inert
            // without this call.
            m_camera.Tick(dt);

            int fbWidth, fbHeight;
            glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
            m_renderer.BeginFrame(fbWidth, fbHeight);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            float aspect = fbHeight > 0 ? static_cast<float>(fbWidth) / fbHeight : 1.0f;
            glm::mat4 view = m_camera.GetViewMatrix();
            glm::mat4 projection = m_camera.GetProjectionMatrix(aspect);
            glm::mat4 model = glm::mat4(1.0f);

            // Phase 14: looked up once here (moved up from where the
            // Inspector used to compute it) so both the 3D highlight below
            // and the Inspector panel further down use the same lookup.
            const Zone* selectedZone = m_selectedZoneId >= 0
                ? m_digitalTwin.FindZone(m_selectedZoneId)
                : nullptr;

            // Phase 14: rebuild the highlight-frame mesh only when the
            // selection actually changed since last frame — see the
            // m_highlightMeshZoneId doc comment in Application.h.
            if (m_selectedZoneId != m_highlightMeshZoneId) {
                m_highlightMeshZoneId = m_selectedZoneId;
                if (selectedZone) {
                    m_selectionHighlightMesh = SelectionHighlight::Build(*selectedZone);
                    m_hasHighlightMesh = true;
                } else {
                    m_hasHighlightMesh = false;
                }
            }

            m_gridShader.Use();
            m_gridShader.SetMat4("uModel", model);
            m_gridShader.SetMat4("uView", view);
            m_gridShader.SetMat4("uProjection", projection);

            m_gridShader.SetInt("uUseDataColor", 0);
            m_gridShader.SetVec4("uBaseColor", glm::vec4(0.35f, 0.55f, 0.35f, 1.0f));
            m_groundGrid.Draw();

            if (m_hasCityData) {
                // Phase 9: buildings and green areas are colored by
                // m_activeLayer — their vertex dataValue was baked in by
                // RebuildCityMeshForActiveLayer() (see CityMeshBuilder /
                // HeatLayers.h / basic.frag's DataRamp). uBaseColor's rgb is
                // ignored in this mode; only its alpha is used, so it's set
                // to opaque white here.
                m_gridShader.SetInt("uUseDataColor", 1);
                m_gridShader.SetVec4("uBaseColor", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
                m_city.buildings.Draw();
                m_city.greenAreas.Draw();

                // Roads and facility markers keep their flat Phase 3
                // category colors — neither belongs to a single zone the
                // way a building or green area does.
                m_gridShader.SetInt("uUseDataColor", 0);
                m_gridShader.SetVec4("uBaseColor", glm::vec4(0.20f, 0.20f, 0.22f, 1.0f));  // roads: asphalt
                m_city.roads.Draw();

                m_gridShader.SetVec4("uBaseColor", glm::vec4(0.85f, 0.35f, 0.15f, 1.0f));  // facility markers
                m_city.facilities.Draw();
            }

            if (m_hasHighlightMesh) {
                // Phase 14: gentle pulse between two brightness levels so
                // the selected zone reads clearly against the data-layer
                // colors on the buildings inside it, without the animation
                // itself becoming the focal point — master spec: "do not
                // make the animation distracting."
                float pulse = 0.75f + 0.25f * std::sin(currentFrameTime * 2.0f);
                m_gridShader.SetInt("uUseDataColor", 0);
                m_gridShader.SetVec4("uBaseColor", glm::vec4(pulse, pulse, pulse, 1.0f));
                m_selectionHighlightMesh.Draw();
            }

            // Phase 10: Zone Inspector panel, drawn over the 3D scene each
            // frame from whichever zone the crosshair last selected.
            Inspector::Render(selectedZone);

            // Phase 11: Top Priority Zones panel. Clicking an entry selects
            // that zone and snaps the camera to it — the Inspector above
            // will show it starting next frame since m_selectedZoneId is
            // already updated by the time this frame finishes.
            int clickedPriorityZoneId = Inspector::RenderTopPriorityPanel(m_digitalTwin.Zones());
            if (clickedPriorityZoneId >= 0) {
                FocusCameraOnZone(clickedPriorityZoneId);
            }

            // Phase 13: City Layers panel — same underlying switch as the
            // Phase 9 hotkeys (1-5, L), just clickable. A click here goes
            // through the same SetActiveLayer() as the keyboard path, so
            // the mesh rebuild and console legend log stay identical
            // regardless of which input triggered it.
            std::optional<DataLayer> clickedLayer = LayersPanel::Render(m_activeLayer);
            if (clickedLayer.has_value()) {
                SetActiveLayer(*clickedLayer);
            }

            // Phase 14: legend for whichever layer LayersPanel/the hotkeys
            // just selected — always reflects m_activeLayer, so it can
            // never show a stale gradient for a layer that's no longer on
            // screen.
            HeatLegend::Render(m_activeLayer);

            // Phase 15: changes propagate through temperature, risk,
            // population exposure and priority before the next frame.
            if (ScenarioPanel::Render(m_scenario, m_digitalTwin.Zones(),
                m_weather.valid ? m_weather.currentTemperature : 0.0f,
                m_weather.valid ? m_weather.precipitation : 0.0f)) {
                ApplyHeatwaveScenario();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(m_window);
        }
    }

    void Application::ProcessInput(float dt) {
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, true);
        }
        if (glfwGetKey(m_window, GLFW_KEY_R) == GLFW_PRESS) {
            m_camera.Reset();  // Reset() always lands in FreeFly (see its doc comment)
            ApplyCursorModeForCurrentCamera();
        }

        bool forward = glfwGetKey(m_window, GLFW_KEY_W) == GLFW_PRESS;
        bool backward = glfwGetKey(m_window, GLFW_KEY_S) == GLFW_PRESS;
        bool left = glfwGetKey(m_window, GLFW_KEY_A) == GLFW_PRESS;
        bool right = glfwGetKey(m_window, GLFW_KEY_D) == GLFW_PRESS;
        bool up = glfwGetKey(m_window, GLFW_KEY_SPACE) == GLFW_PRESS;
        bool down = glfwGetKey(m_window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

        m_camera.ProcessKeyboard(forward, backward, left, right, up, down, dt);
    }

    void Application::Shutdown() {
        if (m_window) {
            ShutdownImGui();  // must run while the GL context is still current
            glfwDestroyWindow(m_window);
            m_window = nullptr;
            glfwTerminate();
        }
    }

    void Application::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
        glViewport(0, 0, width, height);
    }

    void Application::CursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (!app) return;

        CameraMode mode = app->m_camera.GetMode();

        if (mode == CameraMode::FreeFly) {
            // Unchanged Phase 0 behavior: raw unbounded delta while the
            // cursor is captured for FPS-style look.
            if (!app->m_mouseLookEnabled) return;

            if (app->m_firstMouse) {
                app->m_lastMouseX = static_cast<float>(xpos);
                app->m_lastMouseY = static_cast<float>(ypos);
                app->m_firstMouse = false;
            }

            float xOffset = static_cast<float>(xpos) - app->m_lastMouseX;
            float yOffset = app->m_lastMouseY - static_cast<float>(ypos);  // inverted y
            app->m_lastMouseX = static_cast<float>(xpos);
            app->m_lastMouseY = static_cast<float>(ypos);

            app->m_camera.ProcessMouseMovement(xOffset, yOffset);
            return;
        }

        // Phase 12: Orbit/TopDown/Isometric drive off a normal, visible
        // cursor instead — the delta below is a real on-screen pixel
        // delta since the last frame, not an unbounded FPS-look delta.
        // Skipped while an ImGui panel wants the mouse, so dragging over
        // e.g. the Top Priority Zones list doesn't also spin the camera.
        if (ImGui::GetIO().WantCaptureMouse) {
            app->m_lastDragX = xpos;
            app->m_lastDragY = ypos;
            return;
        }

        double dx = xpos - app->m_lastDragX;
        double dy = ypos - app->m_lastDragY;
        app->m_lastDragX = xpos;
        app->m_lastDragY = ypos;

        if (!app->m_leftMouseDown && !app->m_rightMouseDown) return;

        // A few pixels of movement while a button is held counts as a real
        // drag, not a click — used by MouseButtonCallback to decide whether
        // releasing the left button should also fire a zone pick.
        if (std::fabs(dx) + std::fabs(dy) > 3.0) {
            app->m_didDragThisPress = true;
        }

        if (mode == CameraMode::Orbit) {
            // Left-drag rotates, right-drag pans — see Camera.h's
            // OrbitDrag()/PanDrag() doc comments.
            if (app->m_leftMouseDown) {
                app->m_camera.OrbitDrag(static_cast<float>(dx), static_cast<float>(dy));
            } else if (app->m_rightMouseDown) {
                app->m_camera.PanDrag(static_cast<float>(dx), static_cast<float>(dy));
            }
        } else {
            // TopDown/Isometric hold a fixed viewing angle by design
            // (Camera.h), so either button just pans.
            app->m_camera.PanDrag(static_cast<float>(dx), static_cast<float>(dy));
        }
    }

    void Application::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (!app) return;
        app->m_camera.ProcessScroll(static_cast<float>(yoffset));
    }

    void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        // Phase 9: layer toggles. These are discrete (non-held) key events —
        // continuous movement stays in ProcessInput()'s per-frame polling,
        // same split this function's original comment already called for.
        if (action != GLFW_PRESS) return;

        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (!app) return;

        switch (key) {
        case GLFW_KEY_1: app->SetActiveLayer(DataLayer::HeatRisk); break;
        case GLFW_KEY_2: app->SetActiveLayer(DataLayer::Population); break;
        case GLFW_KEY_3: app->SetActiveLayer(DataLayer::PopulationExposure); break;
        case GLFW_KEY_4: app->SetActiveLayer(DataLayer::GreenCoverage); break;
        case GLFW_KEY_5: app->SetActiveLayer(DataLayer::BuildingDensity); break;
        case GLFW_KEY_6: app->SetActiveLayer(DataLayer::Temperature); break;
        case GLFW_KEY_7: app->SetActiveLayer(DataLayer::FloodRisk); break;
        case GLFW_KEY_8: app->SetActiveLayer(DataLayer::SatelliteEnvironment); break;
        case GLFW_KEY_L: app->SetActiveLayer(NextDataLayer(app->m_activeLayer)); break;

        // Phase 12: camera-mode hotkeys. All four route through
        // SetCameraMode() so the cursor capture state stays correct and
        // the switch always animates (Camera::SetMode()).
        case GLFW_KEY_F: app->SetCameraMode(CameraMode::FreeFly);   break;
        case GLFW_KEY_O: app->SetCameraMode(CameraMode::Orbit);     break;
        case GLFW_KEY_T: app->SetCameraMode(CameraMode::TopDown);   break;
        case GLFW_KEY_I: app->SetCameraMode(CameraMode::Isometric); break;

        case GLFW_KEY_TAB: {
            // Only meaningful in FreeFly (see ApplyCursorModeForCurrentCamera) —
            // every other mode already runs with a free cursor. Still safe
            // to toggle the flag regardless of current mode: it simply takes
            // effect next time the camera is FreeFly.
            app->m_mouseLookEnabled = !app->m_mouseLookEnabled;
            app->ApplyCursorModeForCurrentCamera();
            LogInfo(std::string("Mouse capture ") +
                (app->m_mouseLookEnabled ? "ON (camera look)" : "OFF (cursor free for UI)"));
            break;
        }
        default: break;

        }
    }

}  // namespace twin
