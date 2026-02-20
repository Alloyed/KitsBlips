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

out vec2 interpolatedTextureCoordinates;

void main() {
    interpolatedTextureCoordinates = textureCoordinates;

    gl_Position = position;
}
        )HERE");

        GL::Shader frag(GL::Version::GL330, GL::Shader::Type::Fragment);
        frag.addSource(R"HERE(
#extension GL_ARB_explicit_uniform_location: require
layout(location = 0) uniform vec3 color = vec3(1.0, 1.0, 1.0);
layout(location = 1) uniform sampler2D textureData;

in vec2 interpolatedTextureCoordinates;

out vec4 fragmentColor;

void main() {
    //fragmentColor.rgb = color*texture(textureData, interpolatedTextureCoordinates).bgr;
    fragmentColor.rgb = color*texture(textureData, interpolatedTextureCoordinates).rgb;
    //fragmentColor.rgb = interpolatedTextureCoordinates.xyx;
    fragmentColor.a = 1.0;
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

    PostProcessShader& bindColor(const Color3& color) {
        setUniform(0, Vector3(color.r(), color.g(), color.b()));
        return *this;
    }

    PostProcessShader& bindColor(GL::Texture2D& tex) {
        tex.bind(1);
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
            texColor.setStorage(1, GL::TextureFormat::RGBA8, size);
            texDepthAndStencil.setStorage(1, GL::TextureFormat::Depth24Stencil8, size);
            framebuffer.setViewport({{}, size});
        }
    }

    GL::AbstractFramebuffer& startDraw() {
        if (!mEnabled) {
            GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
            GL::defaultFramebuffer.mapForDraw({{0, GL::DefaultFramebuffer::DrawAttachment::Back}});
            GL::defaultFramebuffer.bind();
            return GL::defaultFramebuffer;
        }
        setSize(GL::defaultFramebuffer.viewport().size());
        framebuffer.attachTexture(GL::Framebuffer::ColorAttachment(0), texColor, 0);
        framebuffer.attachTexture(GL::Framebuffer::BufferAttachment::DepthStencil, texDepthAndStencil, 0);
        framebuffer.mapForDraw({
            {Shaders::PhongGL::ColorOutput, GL::Framebuffer::ColorAttachment(0)},
        });
        framebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
        framebuffer.bind();
        return framebuffer;
    }

    void finishDraw() {
        if (!mEnabled) {
            return;
        }
        GL::defaultFramebuffer.clear(GL::FramebufferClear::Color | GL::FramebufferClear::Depth);
        GL::defaultFramebuffer.mapForDraw({{0, GL::DefaultFramebuffer::DrawAttachment::Back}});
        GL::defaultFramebuffer.bind();
        shader.bindColor(texColor).draw(fullscreenQuad);
    }

   private:
    // Working on it :p
    // bool mEnabled = true;
    bool mEnabled = false;
    GL::Framebuffer framebuffer{{{}, {}}};
    GL::Texture2D texColor;
    GL::Texture2D texDepthAndStencil;
    PostProcessShader shader;
    GL::Buffer quadPosVertices;
    GL::Buffer quadTexVertices;
    GL::Mesh fullscreenQuad;
};
}  // namespace kitgui::gfx