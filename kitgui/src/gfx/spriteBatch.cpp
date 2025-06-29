#include "gfx/spriteBatch.h"
#include <glad/gl.h>

namespace {
const std::array<float, 12> kQuadVertices = {
    0.5f,  0.5f,  0.0f,  // top right
    0.5f,  -0.5f, 0.0f,  // bottom right
    -0.5f, -0.5f, 0.0f,  // bottom left
    -0.5f, 0.5f,  0.0f   // top left
};
const std::array<GLuint, 6> kQuadIndices = {
    // note that we start from 0!
    0, 1, 3,  // first triangle
    1, 2, 3   // second triangle
};
}  // namespace

namespace kitgui {
void TextureList::GLState::Bind(const GladGLContext& gl) {
    gl.BindVertexArray(vao);

    gl.GenBuffers(1, &vertexBuffer);
    gl.BindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
    gl.BufferData(GL_ARRAY_BUFFER, kQuadVertices.size(), kQuadVertices.data(), GL_STATIC_DRAW);

    gl.GenBuffers(1, &indexBuffer);
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, kQuadIndices.size(), kQuadIndices.data(), GL_STATIC_DRAW);
}
void TextureList::GLState::BindTexture(const GladGLContext& gl) {
    // fixme: Use material's baseColor
}
void TextureList::GLState::Unbind() {}

void TextureList::GLState::EndDraw() {
    const GLuint numVertices = kQuadVertices.size();
    gl->DrawElements(GL_TRIANGLES, numVertices, GL_UNSIGNED_INT, 0);
}
}  // namespace kitgui