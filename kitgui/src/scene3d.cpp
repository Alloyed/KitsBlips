#include "scene3d.h"

#include <glad/gl.h>

#include <SDL3/SDL_opengl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include "battery/embed.hpp"
#include "shaders.h"

/*
 *  this code is adapted from tinygltfs example code at:
 *   https://github.com/syoyo/tinygltf/blob/release/examples/basic/main.cpp
 *  TODO:
 *  - not-iostream logging
 *  - track if loading did or didn't work
 *  - load from plugin binary directly (physfs?)
 */

#define BUFFER_OFFSET(i) (reinterpret_cast<void*>(i))

namespace {

void bindMesh(const GladGLContext& gl, std::map<int, GLuint>& vbos, tinygltf::Model& model, tinygltf::Mesh& mesh) {
    for (size_t i = 0; i < model.bufferViews.size(); ++i) {
        const tinygltf::BufferView& bufferView = model.bufferViews[i];
        if (bufferView.target == 0) {  // TODO impl drawarrays
            std::cout << "WARN: bufferView.target is zero" << std::endl;
            continue;  // Unsupported bufferView.
        }

        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];
        std::cout << "bufferview.target " << bufferView.target << std::endl;

        GLuint vbo;
        gl.GenBuffers(1, &vbo);
        vbos[i] = vbo;
        gl.BindBuffer(bufferView.target, vbo);

        std::cout << "buffer.data.size = " << buffer.data.size()
                  << ", bufferview.byteOffset = " << bufferView.byteOffset << std::endl;

        gl.BufferData(bufferView.target, bufferView.byteLength, &buffer.data.at(0) + bufferView.byteOffset,
                      GL_STATIC_DRAW);
    }

    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

        for (auto& attrib : primitive.attributes) {
            tinygltf::Accessor accessor = model.accessors[attrib.second];
            int byteStride = accessor.ByteStride(model.bufferViews[accessor.bufferView]);
            gl.BindBuffer(GL_ARRAY_BUFFER, vbos[accessor.bufferView]);

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
                gl.EnableVertexAttribArray(vaa);
                gl.VertexAttribPointer(vaa, size, accessor.componentType, accessor.normalized ? GL_TRUE : GL_FALSE,
                                       byteStride, BUFFER_OFFSET(accessor.byteOffset));
            } else
                std::cout << "vaa missing: " << attrib.first << std::endl;
        }

        if (model.textures.size() > 0) {
            // fixme: Use material's baseColor
            tinygltf::Texture& tex = model.textures[0];

            if (tex.source > -1) {
                GLuint texid;
                gl.GenTextures(1, &texid);

                tinygltf::Image& image = model.images[tex.source];

                gl.BindTexture(GL_TEXTURE_2D, texid);
                gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
                gl.TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                gl.TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

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

                gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, format, type,
                              &image.image.at(0));
            }
        }
    }
}

// bind models
void bindModelNodes(const GladGLContext& gl,
                    std::map<int, GLuint>& vbos,
                    tinygltf::Model& model,
                    tinygltf::Node& node) {
    if ((node.mesh >= 0) && (static_cast<size_t>(node.mesh) < model.meshes.size())) {
        bindMesh(gl, vbos, model, model.meshes[node.mesh]);
    }

    for (size_t i = 0; i < node.children.size(); i++) {
        assert((node.children[i] >= 0) && (static_cast<size_t>(node.children[i]) < model.nodes.size()));
        bindModelNodes(gl, vbos, model, model.nodes[node.children[i]]);
    }
}

std::pair<GLuint, std::map<int, GLuint>> bindModel(const GladGLContext& gl, tinygltf::Model& model) {
    std::map<int, GLuint> vbos;
    GLuint vao;
    gl.GenVertexArrays(1, &vao);
    gl.BindVertexArray(vao);

    const tinygltf::Scene& scene = model.scenes[model.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        assert((scene.nodes[i] >= 0) && (static_cast<size_t>(scene.nodes[i]) < model.nodes.size()));
        bindModelNodes(gl, vbos, model, model.nodes[scene.nodes[i]]);
    }

    gl.BindVertexArray(0);
    // cleanup vbos but do not delete index buffers yet
    for (auto it = vbos.cbegin(); it != vbos.cend();) {
        tinygltf::BufferView bufferView = model.bufferViews[it->first];
        if (bufferView.target != GL_ELEMENT_ARRAY_BUFFER) {
            gl.DeleteBuffers(1, &vbos[it->first]);
            vbos.erase(it++);
        } else {
            ++it;
        }
    }

    return {vao, vbos};
}

void drawMesh(const GladGLContext& gl,
              const std::map<int, GLuint>& vbos,
              tinygltf::Model& model,
              tinygltf::Mesh& mesh) {
    for (size_t i = 0; i < mesh.primitives.size(); ++i) {
        tinygltf::Primitive primitive = mesh.primitives[i];
        tinygltf::Accessor indexAccessor = model.accessors[primitive.indices];

        gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbos.at(indexAccessor.bufferView));

        gl.DrawElements(primitive.mode, indexAccessor.count, indexAccessor.componentType,
                        BUFFER_OFFSET(indexAccessor.byteOffset));
    }
}

// recursively draw node and children nodes of model
void drawModelNodes(const GladGLContext& gl,
                    const std::pair<GLuint, std::map<int, GLuint>>& vaoAndEbos,
                    tinygltf::Model& model,
                    tinygltf::Node& node) {
    if ((node.camera >= 0) && (static_cast<size_t>(node.camera) < model.cameras.size())) {
        // TODO: apply camera
    }

    if ((node.mesh >= 0) && (static_cast<size_t>(node.mesh) < model.meshes.size())) {
        drawMesh(gl, vaoAndEbos.second, model, model.meshes[node.mesh]);
    }
    for (size_t i = 0; i < node.children.size(); i++) {
        drawModelNodes(gl, vaoAndEbos, model, model.nodes[node.children[i]]);
    }
}
}  // namespace

namespace kitgui {
Scene3d::Scene3d() {
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    const auto sceneFile = b::embed<"assets/duck.glb">();
    bool res = loader.LoadBinaryFromMemory(&mModel, &err, &warn, reinterpret_cast<const uint8_t*>(sceneFile.data()),
                                           sceneFile.size());
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
    if (!mVaoAndEbos.first) {
        mVaoAndEbos = bindModel(gl, mModel);
    }
}

Scene3d::~Scene3d() {
    if (mVaoAndEbos.first) {
        // mGl.DeleteVertexArrays(1, &mVaoAndEbos.first);
    }
}

void Scene3d::Draw(const GladGLContext& gl) {
    // Model matrix : an identity matrix (model will be at the origin)

    glm::mat4 rotation_mat =
        glm::rotate(glm::mat4(1.0f), glm::radians(0.8f), glm::vec3(0, 1, 0));  // rotate model on y axis
    glm::vec3 model_pos = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::mat4 translation_mat = glm::translate(glm::mat4(1.0f), model_pos);  // reposition model

    glm::mat4 model_mat = translation_mat * rotation_mat;

    // generate a camera view, based on eye-position and lookAt world-position
    glm::mat4 view_mat = glm::lookAt(glm::vec3(0.0f, 0.0f, 40),    // Camera position, in World Space
                                     glm::vec3(0.0f, 0.0f, 0.0f),  // looking at origin
                                     glm::vec3(0, 1, 0)            // Head is up (set to 0,-1,0 to look upside-down)
    );

    // generate a projection matrix
    float aspectRatio = 1.0f;  // width / height
    float horizontalFovAt169 = glm::radians(90.0f);
    float verticalFov = 2 * glm::atan(glm::tan(horizontalFovAt169 / 2.0f) * 9.0f / 16.0f);
    float zNear = 0.01f;
    float zFar = 1000.0f;
    glm::mat4 proj_mat = glm::perspective(verticalFov, aspectRatio, zNear, zFar);

    glm::vec3 sun_position = glm::vec3(3.0, 10.0, 5.0);
    glm::vec3 sun_color = glm::vec3(1.0);

    mShaders.Use(gl, model_mat, view_mat, proj_mat, sun_position, sun_color);
    gl.BindVertexArray(mVaoAndEbos.first);

    const tinygltf::Scene& scene = mModel.scenes[mModel.defaultScene];
    for (size_t i = 0; i < scene.nodes.size(); ++i) {
        drawModelNodes(gl, mVaoAndEbos, mModel, mModel.nodes[scene.nodes[i]]);
    }

    gl.BindVertexArray(0);
}
}  // namespace kitgui