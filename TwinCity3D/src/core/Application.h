#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>

#include "Camera.h"
#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

namespace twin {

// Owns window lifecycle, the render loop, and top-level input handling.
// Phase 0 responsibility only: open a window and render a basic 3D scene
// (a ground grid, viewed through a working FPS camera). GIS content,
// heat-risk layers, and UI panels attach to this class in later phases.
class Application {
public:
    Application(int width, int height, const std::string& title)
        : m_width(width), m_height(height), m_title(title) {}

    ~Application() { Shutdown(); }

    bool Init();
    void Run();
    void Shutdown();

private:
    void ProcessInput(float dt);

    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

    GLFWwindow* m_window{nullptr};
    int m_width;
    int m_height;
    std::string m_title;

    Camera m_camera;
    Renderer m_renderer;
    Shader m_gridShader;
    Mesh m_groundGrid;

    bool m_firstMouse{true};
    float m_lastMouseX{0.0f};
    float m_lastMouseY{0.0f};
    bool m_mouseLookEnabled{true};
};

}  // namespace twin
