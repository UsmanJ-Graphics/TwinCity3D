cmake_minimum_required(VERSION 3.16)
project(LahoreDigitalTwin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)

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

    # gis
    src/gis/GISLoader.cpp
    src/gis/Triangulate.h

    # twin
    src/twin/CityMeshBuilder.cpp
    src/twin/DigitalTwin.cpp
    src/twin/WeatherLoader.cpp   # Phase 5, Step 2 (new)
)

target_include_directories(LahoreDigitalTwin PRIVATE
    src
    external
)

target_link_libraries(LahoreDigitalTwin PRIVATE
    OpenGL::GL
    glfw
    GLEW::GLEW
    glm::glm
)
