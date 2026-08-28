#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <vector>

namespace twin {

    // A single vertex: position + normal + a scalar "data" channel + a scalar
    // "material variation" channel. The data channel is deliberately generic —
    // later phases reuse it to carry per-vertex heat-risk value, population
    // density, etc. without changing the mesh format, since the digital twin
    // colors geometry by whichever data layer is active rather than by a fixed
    // per-mesh color. materialVariation (Phase 12) is a separate, independent
    // channel: a small per-building pseudo-random value baked in by
    // CityMeshBuilder so buildings colored by the same data-layer value don't
    // all render as one perfectly flat, identical block of color — see
    // basic.frag's use of it. Existing code that constructs a Vertex with only
    // {position, normal, dataValue} still compiles unchanged: materialVariation
    // falls back to its default of 0.0f for anything that doesn't set it
    // (roads, green areas, facility markers).
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        float dataValue{ 0.0f };
        float materialVariation{ 0.0f };
    };

    // Minimal indexed-triangle mesh wrapper. Buildings, roads, and green areas
    // all resolve to this same representation after GIS preprocessing.
    class Mesh {
    public:
        Mesh() = default;
        ~Mesh() { Destroy(); }

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        Mesh(Mesh&& other) noexcept { MoveFrom(other); }
        Mesh& operator=(Mesh&& other) noexcept {
            if (this != &other) {
                Destroy();
                MoveFrom(other);
            }
            return *this;
        }

        void Upload(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
            Destroy();
            m_indexCount = static_cast<GLsizei>(indices.size());

            glGenVertexArrays(1, &m_vao);
            glGenBuffers(1, &m_vbo);
            glGenBuffers(1, &m_ebo);

            glBindVertexArray(m_vao);

            glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, dataValue));

            // Phase 12: materialVariation was already part of Vertex and
            // already uploaded into the VBO, but this slot was never
            // enabled, so basic.vert/frag had no way to read it — buildings
            // colored by the same data-layer value rendered as one flat,
            // identical block of color. Wiring it up here is what actually
            // lets CityMeshBuilder's per-building variation reach the screen.
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, materialVariation));

            glBindVertexArray(0);
        }

        void Draw() const {
            if (!m_vao) return;
            glBindVertexArray(m_vao);
            glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

    private:
        void Destroy() {
            if (m_ebo) glDeleteBuffers(1, &m_ebo);
            if (m_vbo) glDeleteBuffers(1, &m_vbo);
            if (m_vao) glDeleteVertexArrays(1, &m_vao);
            m_vao = m_vbo = m_ebo = 0;
            m_indexCount = 0;
        }

        void MoveFrom(Mesh& other) {
            m_vao = other.m_vao;
            m_vbo = other.m_vbo;
            m_ebo = other.m_ebo;
            m_indexCount = other.m_indexCount;
            other.m_vao = other.m_vbo = other.m_ebo = 0;
            other.m_indexCount = 0;
        }

        GLuint m_vao{ 0 };
        GLuint m_vbo{ 0 };
        GLuint m_ebo{ 0 };
        GLsizei m_indexCount{ 0 };
    };

    // Builds a simple ground-plane grid mesh, used in Phase 0 to confirm the
    // render loop and camera work before any real GIS geometry exists.
    inline Mesh MakeGroundGrid(float halfExtent = 200.0f, int divisions = 20) {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        float step = (halfExtent * 2.0f) / divisions;
        int verticesPerRow = divisions + 1;

        for (int row = 0; row <= divisions; ++row) {
            for (int col = 0; col <= divisions; ++col) {
                Vertex v;
                v.position = glm::vec3(-halfExtent + col * step, 0.0f, -halfExtent + row * step);
                v.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                v.dataValue = 0.0f;
                vertices.push_back(v);
            }
        }

        for (int row = 0; row < divisions; ++row) {
            for (int col = 0; col < divisions; ++col) {
                unsigned int topLeft = row * verticesPerRow + col;
                unsigned int topRight = topLeft + 1;
                unsigned int bottomLeft = (row + 1) * verticesPerRow + col;
                unsigned int bottomRight = bottomLeft + 1;

                indices.push_back(topLeft);
                indices.push_back(bottomLeft);
                indices.push_back(topRight);

                indices.push_back(topRight);
                indices.push_back(bottomLeft);
                indices.push_back(bottomRight);
            }
        }

        Mesh mesh;
        mesh.Upload(vertices, indices);
        return mesh;
    }

}  // namespace twin