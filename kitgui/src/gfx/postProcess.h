#pragma once

#include <Corrade/Tags.h>
#include <Magnum/GL/AbstractFramebuffer.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/BufferImage.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/Shaders/PhongGL.h>
#include <cassert>

namespace kitgui::gfx {
using namespace Magnum;
class PostProcessShader : public GL::AbstractShaderProgram {
   public:
    typedef GL::Attribute<0, Vector2> Position;
    typedef GL::Attribute<1, Vector2> Texcoord;

    PostProcessShader() {
        GL::Shader vert(GL::Version::GL330, GL::Shader::Type::Vertex);
        vert.addSource(R"HERE(
layout(location = 0) in vec4 position;
layout(location = 1) in vec2 textureCoordinates;

out vec2 uv;

void main() {
    uv = textureCoordinates;

    gl_Position = position;
}
        )HERE");

        GL::Shader frag(GL::Version::GL330, GL::Shader::Type::Fragment);
        frag.addSource(R"HERE(
#extension GL_ARB_explicit_uniform_location: require
layout(location = 0) uniform sampler2D textureData;
layout(location = 1) uniform float exposure;
layout(location = 2) uniform float gamma;

in vec2 uv;

out vec4 fragmentColor;

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 tonemapping_ACES(const vec3 x) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

void main() {
    vec3 hdrColor = texture(textureData, uv).rgb;
  
    // exposure tone mapping
    vec3 mapped = tonemapping_ACES(hdrColor * exposure);

    // gamma correction 
    mapped = pow(mapped, vec3(1.0 / gamma));
  
    fragmentColor = vec4(mapped, 1.0);
}
        )HERE");

        if (!(vert.compile() && frag.compile())) {
            // throw
        }

        attachShaders({vert, frag});

        if (!link()) {
            // throw
        }
    }

    PostProcessShader& bindColor(GL::Texture2D& tex) {
        tex.bind(0);
        return *this;
    }
    PostProcessShader& bindExposure(float exposure) {
        setUniform(1, exposure);
        return *this;
    }
    PostProcessShader& bindGamma(float gamma) {
        setUniform(2, gamma);
        return *this;
    }

    MAGNUM_GL_ABSTRACTSHADERPROGRAM_SUBCLASS_DRAW_IMPLEMENTATION(PostProcessShader);
};

class PostProcessCanvas {
   public:
    PostProcessCanvas() {
        const Vector2 posVertices[] = {{1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, -1.0f}, {-1.0f, 1.0f}};
        quadPosVertices.setData(posVertices, GL::BufferUsage::StaticDraw);

        const Vector2 texVertices[] = {{1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}};
        quadTexVertices.setData(texVertices, GL::BufferUsage::StaticDraw);

        fullscreenQuad.setPrimitive(GL::MeshPrimitive::TriangleStrip)
            .setCount(4)
            .addVertexBuffer(quadPosVertices, 0, PostProcessShader::Position())
            .addVertexBuffer(quadTexVertices, 0, PostProcessShader::Texcoord());
    }

    void setSize(Vector2i size) {
        if (size != framebuffer.viewport().size()) {
            texColor.setStorage(1, GL::TextureFormat::RGBA16F, size);
            texDepthAndStencil.setStorage(1, GL::TextureFormat::Depth24Stencil8, size);
            framebuffer.setViewport({{}, size});
        }
    }

    GL::AbstractFramebuffer& startDraw() {
        if (!mEnabled) {
            GL::defaultFramebuffer.bind();
            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
            GL::defaultFramebuffer.mapForDraw({{0, GL::DefaultFramebuffer::DrawAttachment::Back}});
            return GL::defaultFramebuffer;
        }
        setSize(GL::defaultFramebuffer.viewport().size());
        framebuffer.bind();
        framebuffer.attachTexture(GL::Framebuffer::ColorAttachment(0), texColor, 0);
        framebuffer.attachTexture(GL::Framebuffer::BufferAttachment::DepthStencil, texDepthAndStencil, 0);
        framebuffer.mapForDraw({
            {Shaders::PhongGL::ColorOutput, GL::Framebuffer::ColorAttachment(0)},
        });
        framebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
        return framebuffer;
    }

    void finishDraw() {
        if (!mEnabled) {
            return;
        }
        GL::defaultFramebuffer.bind();
        GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
        GL::defaultFramebuffer.mapForDraw({{0, GL::DefaultFramebuffer::DrawAttachment::Back}});
        shader.bindColor(texColor).bindExposure(mExposure).bindGamma(mGamma).draw(fullscreenQuad);
    }

    void setParams(bool enabled, float exposure, float gamma) {
        mEnabled = enabled;
        mExposure = exposure;
        mGamma = gamma;
    }
    GL::Texture2D& getDepthAndStencil() { return texDepthAndStencil; }

   private:
    // Working on it :p
    bool mEnabled = true;
    float mExposure = 1.0f;
    float mGamma = 2.2f;
    // bool mEnabled = false;
    GL::Framebuffer framebuffer{{{}, {}}};
    GL::Texture2D texColor;
    GL::Texture2D texDepthAndStencil;
    PostProcessShader shader;
    GL::Buffer quadPosVertices;
    GL::Buffer quadTexVertices;
    GL::Mesh fullscreenQuad;
};
}  // namespace kitgui::gfx