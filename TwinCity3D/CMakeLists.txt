cmake_minimum_required(VERSION 3.16)
project(LahoreDigitalTwin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Run the binary from the project root so relative paths ("shaders/...",
# "data/processed/...") resolve the same in dev and in the demo.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(glm REQUIRED)

add_executable(LahoreDigitalTwin
    src/main.cpp
    src/core/Application.cpp
    src/core/Shader.cpp
    src/gis/GISLoader.cpp
    src/twin/CityMeshBuilder.cpp
    src/twin/DigitalTwin.cpp
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

if (APPLE)
    target_link_libraries(LahoreDigitalTwin PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()