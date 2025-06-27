#include "scene3d.h"

#include <glad/gl.h>

#include <SDL3/SDL_opengl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>
#include "battery/embed.hpp"

/*
 *  this code is adapted from tinygltfs example code at:
 *   https://github.com/syoyo/tinygltf/blob/release/examples/basic/main.cpp
 *  TODO:
 *  - not-iostream logging
 *  - track if loading did or didn't work
 *  - load from plugin binary directly (physfs?)
 */

#define BUFFER_OFFSET(i) (reinterpret_cast<void*>(i))

namespace kitgui {

void Scene3d::Cache::BuildTransform(tinygltf::Model& model, size_t parentIdx, size_t nodeIdx) {
    const auto& node = model.nodes[nodeIdx];
    auto& transform = mNodes[nodeIdx].transform;
    printf("transform %d -> %d\n", parentIdx, nodeIdx);
    #define A(idx) (static_cast<float>(a[idx]))
    if (!node.matrix.empty()) {
        const auto& a = node.matrix;
        // clang-format off
            printf("full matrix %d", nodeIdx);
            transform = mNodes[parentIdx].transform * glm::mat4(
                A(0),  A(1),  A(2),  A(3),
                A(4),  A(5),  A(6),  A(7),
                A(8),  A(9),  A(10), A(11),
                A(12), A(13), A(14), A(15)
            );
        // clang-format on
    } else {
        transform = mNodes[parentIdx].transform;
        if (!node.translation.empty()) {
            const auto& a = node.translation;
            printf("Translate %d: %f %f %f\n", nodeIdx, A(0), A(1), A(2));
            transform = glm::translate(transform, glm::vec3(A(0), A(1), A(2)));
        }
        if (!node.rotation.empty()) {
            const auto& a = node.rotation;
            printf("Rotate %d: %f %f %f %f\n", nodeIdx, A(0), A(1), A(2), A(3));
            // quat is WXYZ in glm instead of XYZW
            transform = transform * glm::mat4_cast(glm::quat(A(3), A(0), A(1), A(2)));
        }
        if (!node.scale.empty()) {
            const auto& a = node.scale;
            printf("Scale %d: %f %f %f %f\n", nodeIdx, A(0), A(1), A(2), A(3));
            transform = glm::scale(transform, glm::vec3(A(0), A(1), A(2)));
        }
    }
    #undef A
}

void Scene3d::Cache::Build(tinygltf::Model& model) {
    mNodes.resize(model.nodes.size());
    mMeshNodes.clear();
    // TODO: animations

    const auto visit = [&](const auto& self, size_t parentIdx, size_t nodeIdx, bool hasAnimatedAncestor) -> void {
        printf("Visit %d %d\n", parentIdx, nodeIdx);
        BuildTransform(model, parentIdx, nodeIdx);

        const auto& node = model.nodes[nodeIdx];

        if(node.mesh != -1) {
            printf("Mesh %d\n", nodeIdx);
            mMeshNodes.push_back(nodeIdx);
        }
        if(node.camera != -1) {
            printf("Camera %d\n", nodeIdx);
            mCamera = nodeIdx;
        }
        for (const auto childIdx : node.children) {
            self(self, nodeIdx, childIdx, hasAnimatedAncestor);
        }
    };
    const auto& scene = model.scenes[model.defaultScene];
    for (const auto rootIdx : scene.nodes) {
        // start with identity matrix
        mNodes[rootIdx].transform = glm::mat4(1.0f);
        printf("Root %d\n", rootIdx);
        visit(visit, rootIdx, rootIdx, false);
    }
}

void Scene3d::GLState::Bind(const GladGLContext& _gl, tinygltf::Model& model) {
    this->gl = &_gl;

    gl->GenVertexArrays(1, &vertexArray);
    gl->BindVertexArray(vertexArray);

    BindBuffers(model);
    BindMeshes(model);
    BindTextures(model);

    gl->BindVertexArray(0);

    // cleanup vbos but do not delete index buffers yet
    /*
    std::erase_if(elementBuffers, [&](const auto& item) {
        const auto& [key, value] = item;
        const auto& bufferView = model.bufferViews[key];
        if (bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
            gl->DeleteBuffers(1, &value);
            return true;
        } else {
            return false;
        }
    });
    */
}

void Scene3d::GLState::BindBuffers(tinygltf::Model& model) {
        for (size_t bufferViewIdx = 0; bufferViewIdx < model.bufferViews.size(); ++bufferViewIdx) {
        const tinygltf::BufferView& bufferView = model.bufferViews[bufferViewIdx];
        if (bufferView.target == 0) {  // TODO impl drawarrays
            std::cout << "WARN: bufferView.target is zero" << std::endl;
            continue;  // Unsupported bufferView.
        }

        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        std::cout << "bufferview.target " << bufferView.target << std::endl;

        GLuint vbo;
        gl->GenBuffers(1, &vbo);
        elementBuffers[bufferViewIdx] = vbo;
        gl->BindBuffer(bufferView.target, vbo);

        std::cout << "buffer.data.size = " << buffer.data.size()
                  << ", bufferview.byteOffset = " << bufferView.byteOffset << std::endl;

        gl->BufferData(bufferView.target, bufferView.byteLength, &buffer.data.at(0) + bufferView.byteOffset,
                      GL_STATIC_DRAW);
    }

}
void Scene3d::GLState::BindMeshes(tinygltf::Model& model) {
    for (const auto& mesh : model.meshes) {
        for (size_t i = 0; i < mesh.primitives.size(); ++i) {
            tinygltf::Primitive primitive = mesh.primitives[i];
            tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

            for (auto& attrib : primitive.attributes) {
                tinygltf::Accessor accessor = model.accessors[attrib.second];
                int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
                gl->BindBuffer(GL_ARRAY_BUFFER, elementBuffers[accessor.bufferView]);

                int size = 1;
                if (accessor.type != TINYGLTF_TYPE_SCALAR) {
                    size = accessor.type;
                }

                int vaa = -1;
                if (attrib.first.compare("POSITION") == 0)
                    vaa = 0;
                if (attrib.first.compare("NORMAL") == 0)
                    vaa = 1;
                if (attrib.first.compare("TEXCOORD_0") == 0)
                    vaa = 2;
                if (vaa > -1) {
                    gl->EnableVertexAttribArray(vaa);
                    gl->VertexAttribPointer(vaa, size, accessor.componentType, accessor.normalized ? GL_TRUE : GL_FALSE,
                                            byteStride, BUFFER_OFFSET(accessor.byteOffset));
                } else
                    std::cout << "vaa missing: " << attrib.first << std::endl;
            }
        }
    }
}

void Scene3d::GLState::BindTextures(tinygltf::Model& model) {
    if (model.textures.size() > 0) {
        // fixme: Use material's baseColor
        tinygltf::Texture& tex = model.textures[0];

        if (tex.source > -1) {
            GLuint texid;
            gl->GenTextures(1, &texid);

            tinygltf::Image& image = model.images[tex.source];

            gl->BindTexture(GL_TEXTURE_2D, texid);
            gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
            gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            gl->TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            GLenum format = GL_RGBA;

            if (image.component == 1) {
                format = GL_RED;
            } else if (image.component == 2) {
                format = GL_RG;
            } else if (image.component == 3) {
                format = GL_RGB;
            } else {
                // ???
            }

            GLenum type = GL_UNSIGNED_BYTE;
            if (image.bits == 8) {
                // ok
            } else if (image.bits == 16) {
                type = GL_UNSIGNED_SHORT;
            } else {
                // ???
            }

            gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, format, type, &image.image.at(0));
        }
    }
}

void Scene3d::GLState::Unbind() {
    gl->DeleteVertexArrays(1, &vertexArray);
}

void Scene3d::GLState::DrawMesh(tinygltf::Model& model, MeshIdx meshIdx) {
    const auto& mesh = model.meshes[meshIdx];
    for(const auto primitive : mesh.primitives) {
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

        gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementBuffers.at(indexAccessor.bufferView));
        gl->DrawElements(primitive.mode, static_cast<GLsizei>(indexAccessor.count), indexAccessor.componentType,
                        BUFFER_OFFSET(indexAccessor.byteOffset));
    }
}

void Scene3d::GLState::StartDraw() {
    gl->BindVertexArray(vertexArray);
}
void Scene3d::GLState::EndDraw() {
    gl->BindVertexArray(0);
}

Scene3d::Scene3d() {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    const auto sceneFile = b::embed<"assets/duck.glb">();
    bool res = loader.LoadBinaryFromMemory(&mModel, &err, &warn, reinterpret_cast<const uint8_t*>(sceneFile.data()),
                                           static_cast<uint32_t>(sceneFile.size()));
    if (!warn.empty()) {
        std::cout << "WARN: " << warn << std::endl;
    }

    if (!err.empty()) {
        std::cout << "ERR: " << err << std::endl;
    }

    if (!res) {
        std::cout << "Failed to load GLTF " << std::endl;
    }
}

void Scene3d::Bind(const GladGLContext& gl) {
    if(!mGlState.bound)
    {
        mCache.Build(mModel);
        mGlState.Bind(gl, mModel);
        mGlState.bound = true;
    }
}

Scene3d::~Scene3d() {
    if(mGlState.bound)
    {
        mGlState.Unbind();
        mGlState.bound = false;
    }
}

void Scene3d::Draw(const GladGLContext& gl) {
    // generate a projection matrix
    glm::vec3 sun_position = glm::vec3(3.0, 10.0, 5.0);
    glm::vec3 sun_color = glm::vec3(1.0);

    const auto& cam = mModel.cameras[mModel.nodes[mCache.mCamera].camera];
    glm::mat4 proj_mat {};
    if(cam.type == "perspective") {

        const auto& p = cam.perspective;
        proj_mat = glm::perspective(p.yfov, p.aspectRatio, p.znear, p.zfar);
    }
    else {
        const auto& p = cam.orthographic;
        double left {};
        double right {};
        double bottom {};
        double top {};
        proj_mat = glm::ortho(left, right, bottom, top, p.znear, p.zfar);
    }

    glm::mat4 view_mat = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    //glm::mat4 view_mat = mCache.mNodes[mCache.mCamera].transform;
    mGlState.StartDraw();
    for(const auto nodeIdx : mCache.mMeshNodes)
    {
        const auto& node = mModel.nodes[nodeIdx];
        const glm::mat4& model_mat = mCache.mNodes[nodeIdx].transform;
        mShaders.Use(gl, model_mat, view_mat, proj_mat, sun_position, sun_color);
        mGlState.DrawMesh(mModel, node.mesh);
    }
    mGlState.EndDraw();

    gl.BindVertexArray(0);
}
}  // namespace kitgui