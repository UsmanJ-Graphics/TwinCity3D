#include "Application.h"
#include "Log.h"

#include "../gis/GISLoader.h"

#include <glm/gtc/matrix_transform.hpp>

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

        m_renderer.Init();

        if (!m_gridShader.LoadFromFiles("shaders/basic.vert", "shaders/basic.frag")) {
            LogError("Failed to load Phase 0 grid shader");
            return false;
        }

        m_groundGrid = MakeGroundGrid(900.0f, 60);

        LoadCityData();

        LogInfo("Phase 0-3 initialization complete: window + camera + 3D city model ready");
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
                " population=" + (z.populationIsPlaceholder ? "(placeholder)" : std::to_string(z.population)) +
                " temperature=" + (z.temperatureIsPlaceholder ? "(placeholder)" : std::to_string(z.temperature)) +
                " heatRisk=" + (z.riskIsPlaceholder ? "(placeholder)" : std::to_string(z.heatRisk));
            LogInfo(line);
        }
        LogInfo("----------------------------------");
    }

    void Application::Run() {
        float lastFrameTime = static_cast<float>(glfwGetTime());

        while (!glfwWindowShouldClose(m_window)) {
            float currentFrameTime = static_cast<float>(glfwGetTime());
            float dt = currentFrameTime - lastFrameTime;
            lastFrameTime = currentFrameTime;

            glfwPollEvents();
            ProcessInput(dt);

            int fbWidth, fbHeight;
            glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
            m_renderer.BeginFrame(fbWidth, fbHeight);

            float aspect = fbHeight > 0 ? static_cast<float>(fbWidth) / fbHeight : 1.0f;
            glm::mat4 view = m_camera.GetViewMatrix();
            glm::mat4 projection = m_camera.GetProjectionMatrix(aspect);
            glm::mat4 model = glm::mat4(1.0f);

            m_gridShader.Use();
            m_gridShader.SetMat4("uModel", model);
            m_gridShader.SetMat4("uView", view);
            m_gridShader.SetMat4("uProjection", projection);
            m_gridShader.SetVec4("uBaseColor", glm::vec4(0.35f, 0.55f, 0.35f, 1.0f));
            m_groundGrid.Draw();

            if (m_hasCityData) {
                // Category colors only, for now — Phase 9 replaces the building
                // color with a data-driven heat-risk palette per zone/building.
                m_gridShader.SetVec4("uBaseColor", glm::vec4(0.78f, 0.74f, 0.66f, 1.0f));  // buildings: warm concrete
                m_city.buildings.Draw();

                m_gridShader.SetVec4("uBaseColor", glm::vec4(0.20f, 0.20f, 0.22f, 1.0f));  // roads: asphalt
                m_city.roads.Draw();

                m_gridShader.SetVec4("uBaseColor", glm::vec4(0.25f, 0.62f, 0.30f, 1.0f));  // green areas
                m_city.greenAreas.Draw();

                m_gridShader.SetVec4("uBaseColor", glm::vec4(0.85f, 0.35f, 0.15f, 1.0f));  // facility markers
                m_city.facilities.Draw();
            }

            glfwSwapBuffers(m_window);
        }
    }

    void Application::ProcessInput(float dt) {
        if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, true);
        }
        if (glfwGetKey(m_window, GLFW_KEY_R) == GLFW_PRESS) {
            m_camera.Reset();
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
        if (!app || !app->m_mouseLookEnabled) return;

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
    }

    void Application::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
        if (!app) return;
        app->m_camera.ProcessScroll(static_cast<float>(yoffset));
    }

    void Application::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        // Reserved for discrete (non-held) key events, e.g. toggling UI panels
        // in later phases. Continuous movement uses polling in ProcessInput.
    }

}  // namespace twin