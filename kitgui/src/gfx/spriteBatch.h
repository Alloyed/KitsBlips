#pragma once

#include <glad/gl.h>
#include <tiny_gltf.h>

#include "shaders.h"

namespace kitgui {

/*
 * TextureList uploads and draws a series of textured quads
 * While it's perfectly ok to use a texturelist to draw a single texture, perf benefits can be gained by making a
 * texture atlas and batching up textures into a single list.
 */
class TextureList {
   public:
    TextureList();
    ~TextureList();
    void Bind(const GladGLContext& gl);
    void Draw(const GladGLContext& gl);

   private:
    using VaoId = GLuint;
    using ElementBufferId = GLuint;
    // opengl state
    struct GLState {
        const GladGLContext* gl{};
        VaoId vao{};
        ElementBufferId vertexBuffer;
        ElementBufferId indexBuffer;
        bool bound{};

        void Bind(const GladGLContext& gl);
        void BindTexture(const GladGLContext& gl);
        void Unbind();

        void StartDraw();
        void DrawQuad();
        void EndDraw();
    };
    ShaderProgram mShaders;
    GLState mGlState;
};
}  // namespace kitgui