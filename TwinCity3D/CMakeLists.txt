cmake_minimum_required(VERSION 3.16)
project(LahoreDigitalTwin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)

# Phase 10: Dear ImGui, vendored (not fetched) so the hackathon build never
# depends on network access — same "offline-first" rule the master spec
# applies to weather/population data (Phase 22). Drop the Dear ImGui source
# release (docking or master branch, either works) into external/imgui so
# these paths resolve:
#   external/imgui/imgui.cpp
#   external/imgui/backends/imgui_impl_glfw.cpp
#   external/imgui/backends/imgui_impl_opengl3.cpp
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/external/imgui)

add_executable(LahoreDigitalTwin
    src/main.cpp

    # core
    src/core/Application.cpp
    src/core/Camera.h
    src/core/Shader.cpp
    src/core/Mesh.h
    src/core/Renderer.h
    src/core/Texture.h
    src/core/Log.h
    src/core/Picking.h        # Phase 10
    src/core/Picking.cpp      # Phase 10

    # ui
    src/ui/Inspector.h        # Phase 10
    src/ui/Inspector.cpp      # Phase 10

    # gis
    src/gis/GISLoader.cpp
    src/gis/Triangulate.h

    # twin
    src/twin/CityMeshBuilder.cpp
    src/twin/DigitalTwin.cpp
    src/twin/WeatherLoader.cpp   # Phase 5, Step 2
    src/twin/PopulationLoader.cpp   # Phase 7 (was missing — Application.cpp already used it)
    src/twin/HeatRiskModel.cpp   # Phase 8 (new)

    # Dear ImGui (Phase 10) — core + GLFW/OpenGL3 backends
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(LahoreDigitalTwin PRIVATE
    src
    external
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

target_link_libraries(LahoreDigitalTwin PRIVATE
    OpenGL::GL
    glfw
    GLEW::GLEW
    glm::glm
)
