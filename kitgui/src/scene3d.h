#pragma once

#include <glad/gl.h>
#include <tiny_gltf.h>
#include <unordered_map>

#include "shaders.h"


namespace kitgui {
class Scene3d {
   public:
    Scene3d();
    ~Scene3d();
    void Bind(const GladGLContext& gl);
    void Draw(const GladGLContext& gl);

   private:
    using VertexArrayId = GLuint;
    using ElementBufferId = GLuint;

    using BufferViewIdx = size_t;
    using NodeIdx = size_t;
    using MeshIdx = size_t;

    // acceleration structures for the tinygltf::Model instance
    struct Cache {
        struct NodeCache {
            glm::mat4 transform {};
            std::vector<size_t> animationChannels {};
        };
        void Build(tinygltf::Model& model);
        std::vector<NodeCache> mNodes {};
        std::vector<NodeIdx> mMeshNodes {};
        NodeIdx mCamera {};
        private:
        void BuildTransform(tinygltf::Model& model, NodeIdx parentIdx, NodeIdx nodeIdx);
    };

    // opengl state
    struct GLState {
        const GladGLContext* gl {};
        VertexArrayId vertexArray {};
        std::unordered_map<BufferViewIdx, ElementBufferId> elementBuffers {};
        bool bound {};

        void Bind(const GladGLContext& gl, tinygltf::Model& model);
        void BindBuffers(tinygltf::Model& model);
        void BindMeshes(tinygltf::Model& model);
        void BindTextures(tinygltf::Model& model);
        void Unbind();

        void StartDraw();
        void DrawMesh(tinygltf::Model& model, MeshIdx meshIdx);
        void EndDraw();
    };
    ShaderProgram mShaders;
    tinygltf::Model mModel;
    Cache mCache;
    GLState mGlState;
};
}  // namespace kitgui