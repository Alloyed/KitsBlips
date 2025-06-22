#pragma once

#include <glad/gl.h>
#include <tiny_gltf.h>
#include "gui/shaders.h"

namespace kitgui {
class Scene3d {
   public:
    Scene3d(GladGLContext gl);
    ~Scene3d();
    void Bind();
    void Draw();

   private:
    using VertexArrayId = GLuint;
    using ElementBufferId = GLuint;
    GladGLContext mGl;
    tinygltf::Model mModel;
    Shaders mShaders;
    std::pair<VertexArrayId, std::map<int, ElementBufferId>> mVaoAndEbos{};
};
}  // namespace kitgui