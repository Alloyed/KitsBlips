#include "gfx/image.h"

#include <glad/gl.h>
#include <tiny_gltf.h>
#include <cstdint>
#include <span>
// stb_image automatically pulled in by tiny_gltf
#include "stb_image.h"

namespace {
GLuint loadImage_stb(const GladGLContext& gl, std::span<uint8_t> rawFile) {
    int width, height, channels;
    unsigned char* decodedData = stbi_load_from_memory(rawFile.data(), rawFile.size(), &width, &height, &channels, 0);

    GLenum format;
    switch (channels) {
        case 1: {
            format = GL_RED;
            break;
        }
        case 3: {
            format = GL_RGB;
            break;
        }
        case 4: {
            format = GL_RGBA;
            break;
        }
        default: {
            stbi_image_free(decodedData);
            return 0;
        }
    }
    GLenum type = GL_UNSIGNED_BYTE;

    GLuint texid;
    gl.GenTextures(1, &texid);
    gl.BindTexture(GL_TEXTURE_2D, texid);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, format, type, decodedData);

    stbi_image_free(decodedData);
    return texid;
}

#if 0
// TODO: ktx support
GLuint loadImage_ktx(const GladGLContext& gl, std::span<uint8_t> rawFile) {
    ktxTexture* kTexture;
    KTX_error_code result;

    // Load the texture from file
    result = ktxTexture_CreateFromMemory(
        filename.c_str(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        &kTexture
    );

    if (result != KTX_SUCCESS) {
        std::cerr << "Failed to load KTX file: " << ktxErrorString(result) << "\n";
        return 0;
    }

    GLuint textureID;
    GLenum target;
    ktxErrorCode gl_result = ktxTexture_GLUpload(kTexture, &textureID, &target, nullptr);

    if (gl_result != KTX_SUCCESS) {
        std::cerr << "Failed to upload KTX to OpenGL: " << ktxErrorString(gl_result) << "\n";
        ktxTexture_Destroy(kTexture);
        return 0;
    }

    ktxTexture_Destroy(kTexture);
    return textureID;
}
#endif
}  // namespace

namespace kitgui {

GLuint loadImage(const GladGLContext& gl, std::span<uint8_t> rawFile) {
    return loadImage_stb(gl, rawFile);
}

GLuint loadImage(const GladGLContext& gl, tinygltf::Image& image) {
    GLenum format;
    switch (image.component) {
        case 1: {
            format = GL_RED;
            break;
        }
        case 2: {
            format = GL_RG;
            break;
        }
        case 3: {
            format = GL_RGB;
            break;
        }
        case 4: {
            format = GL_RGBA;
            break;
        }
        default: {
            return 0;
        }
    }

    GLenum type;
    switch (image.bits) {
        case 8: {
            type = GL_UNSIGNED_BYTE;
            break;
        }
        case 16: {
            type = GL_UNSIGNED_SHORT;
            break;
        }
        default: {
            return 0;
        }
    }

    GLuint texid;
    gl.GenTextures(1, &texid);
    gl.BindTexture(GL_TEXTURE_2D, texid);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, format, type, &image.image.at(0));

    return texid;
}

}  // namespace kitgui