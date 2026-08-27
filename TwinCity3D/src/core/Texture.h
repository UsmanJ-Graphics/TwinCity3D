#pragma once
#include <GL/glew.h>
#include <array>

namespace twin {

// Phase 0 only needs enough of a texture system to prove the pipeline
// works end-to-end (create -> bind -> sample). Real raster loading (for
// the optional satellite layer in Phase 17) will extend this class rather
// than replace it, so renderer code doesn't need to change later.
class Texture {
public:
    Texture() = default;
    ~Texture() { if (m_id) glDeleteTextures(1, &m_id); }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Creates a 1x1 solid-color texture. Useful as a placeholder / default
    // material and for unit-testing the texture pipeline without any file IO.
    void CreateSolidColor(std::array<unsigned char, 4> rgba) {
        if (m_id) glDeleteTextures(1, &m_id);
        glGenTextures(1, &m_id);
        glBindTexture(GL_TEXTURE_2D, m_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void Bind(unsigned int unit = 0) const {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, m_id);
    }

    GLuint Id() const { return m_id; }

private:
    GLuint m_id{0};
};

}  // namespace twin
