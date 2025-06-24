#pragma once

#include <glad/gl.h>
#include <tiny_gltf.h>
#include "shaders.h"

namespace kitgui {
class Scene3d {
   public:
    Scene3d();
    ~Scene3d();
    void Bind(const GladGLContext& gl);
    void Draw(const GladGLContext& gl);

   private:
    ShaderProgram mShaders;
    using VertexArrayId = GLuint;
    using ElementBufferId = GLuint;
    tinygltf::Model mModel;
    std::pair<VertexArrayId, std::map<int, ElementBufferId>> mVaoAndEbos{};
};
}  // namespace kitgui