#include <glad/gl.h>
#include <tiny_gltf.h>
#include <cstdint>
#include <span>

namespace kitgui {
GLuint loadImage(const GladGLContext& gl, std::span<uint8_t> rawFile);
GLuint loadImage(const GladGLContext& gl, tinygltf::Image& image);
}  // namespace kitgui