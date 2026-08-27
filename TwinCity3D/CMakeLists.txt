cmake_minimum_required(VERSION 3.16)
project(LahoreDigitalTwin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)

# GLM is header-only; locate it via pkg-config or the system path.
find_path(GLM_INCLUDE_DIR glm/glm.hpp PATHS /usr/include /usr/local/include)
if(NOT GLM_INCLUDE_DIR)
    message(FATAL_ERROR "GLM headers not found. Install libglm-dev.")
endif()

file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(LahoreDigitalTwin ${ENGINE_SOURCES})

target_include_directories(LahoreDigitalTwin PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${GLM_INCLUDE_DIR}
)

target_link_libraries(LahoreDigitalTwin PRIVATE
    OpenGL::GL
    glfw
    GLEW::GLEW
)

# Copy shader directory next to the built binary so runtime shader loading
# works regardless of where the executable is invoked from.
add_custom_command(TARGET LahoreDigitalTwin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders
        $<TARGET_FILE_DIR:LahoreDigitalTwin>/shaders
)

message(STATUS "Lahore Urban Heat Digital Twin — build configured (Phase 0)")
