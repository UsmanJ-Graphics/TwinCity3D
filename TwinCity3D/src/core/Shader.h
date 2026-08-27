#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>

namespace twin {

// Loads, compiles, and links a GLSL vertex+fragment shader pair from disk.
// Kept intentionally small for the MVP: no geometry/compute stages, no
// shader hot-reload. Add those later only if the demo needs them.
class Shader {
public:
    Shader() = default;
    ~Shader() { if (m_programId) glDeleteProgram(m_programId); }

    bool LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

    void Use() const { glUseProgram(m_programId); }
    GLuint Id() const { return m_programId; }

    void SetMat4(const std::string& name, const glm::mat4& value) const;
    void SetVec3(const std::string& name, const glm::vec3& value) const;
    void SetVec4(const std::string& name, const glm::vec4& value) const;
    void SetFloat(const std::string& name, float value) const;
    void SetInt(const std::string& name, int value) const;

private:
    static std::string ReadFile(const std::string& path);
    static GLuint CompileStage(GLenum stage, const std::string& source, const std::string& debugName);

    GLuint m_programId{0};
};

}  // namespace twin
