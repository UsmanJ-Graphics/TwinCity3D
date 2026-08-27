#include "Shader.h"
#include "Log.h"

#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

namespace twin {

std::string Shader::ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LogError("Shader file not found: " + path);
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::CompileStage(GLenum stage, const std::string& source, const std::string& debugName) {
    GLuint id = glCreateShader(stage);
    const char* src = source.c_str();
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);

    GLint success = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[1024];
        glGetShaderInfoLog(id, 1024, nullptr, infoLog);
        LogError("Shader compile failed (" + debugName + "): " + infoLog);
        glDeleteShader(id);
        return 0;
    }
    return id;
}

bool Shader::LoadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    std::string vertexSrc = ReadFile(vertexPath);
    std::string fragmentSrc = ReadFile(fragmentPath);
    if (vertexSrc.empty() || fragmentSrc.empty()) return false;

    GLuint vertexId = CompileStage(GL_VERTEX_SHADER, vertexSrc, vertexPath);
    GLuint fragmentId = CompileStage(GL_FRAGMENT_SHADER, fragmentSrc, fragmentPath);
    if (!vertexId || !fragmentId) return false;

    m_programId = glCreateProgram();
    glAttachShader(m_programId, vertexId);
    glAttachShader(m_programId, fragmentId);
    glLinkProgram(m_programId);

    GLint linked = 0;
    glGetProgramiv(m_programId, GL_LINK_STATUS, &linked);
    if (!linked) {
        char infoLog[1024];
        glGetProgramInfoLog(m_programId, 1024, nullptr, infoLog);
        LogError("Shader link failed (" + vertexPath + " + " + fragmentPath + "): " + infoLog);
        glDeleteShader(vertexId);
        glDeleteShader(fragmentId);
        return false;
    }

    glDeleteShader(vertexId);
    glDeleteShader(fragmentId);
    LogInfo("Shader loaded: " + vertexPath + " / " + fragmentPath);
    return true;
}

void Shader::SetMat4(const std::string& name, const glm::mat4& value) const {
    glUniformMatrix4fv(glGetUniformLocation(m_programId, name.c_str()), 1, GL_FALSE, glm::value_ptr(value));
}

void Shader::SetVec3(const std::string& name, const glm::vec3& value) const {
    glUniform3fv(glGetUniformLocation(m_programId, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::SetVec4(const std::string& name, const glm::vec4& value) const {
    glUniform4fv(glGetUniformLocation(m_programId, name.c_str()), 1, glm::value_ptr(value));
}

void Shader::SetFloat(const std::string& name, float value) const {
    glUniform1f(glGetUniformLocation(m_programId, name.c_str()), value);
}

void Shader::SetInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(m_programId, name.c_str()), value);
}

}  // namespace twin
