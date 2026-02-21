#pragma once

#include <Corrade/Tags.h>
#include <Magnum/GL/AbstractFramebuffer.h>
#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/GL/BufferImage.h>
#include <Magnum/GL/DefaultFramebuffer.h>
#include <Magnum/GL/Framebuffer.h>
#include <Magnum/GL/GL.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/GL/Renderer.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Texture.h>
#include <Magnum/GL/TextureFormat.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/PixelFormat.h>
#include <Magnum/SceneGraph/Drawable.h>
#include <Magnum/Shaders/PhongGL.h>
#include <imgui.h>
#include <algorithm>
#include <cassert>

namespace kitgui::gfx {
using namespace Magnum;

// https://blog.frost.kiwi/dual-kawase/#kawase-blur
class KawaseBlurShader : public GL::AbstractShaderProgram {
   public:
    typedef GL::Attribute<0, Vector2> Position;
    typedef GL::Attribute<1, Vector2> Texcoord;

    KawaseBlurShader() {
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
layout(location = 0) uniform sampler2D texture;

layout(location = 1) uniform vec2 frameSizeRCP; /* Resolution Reciprocal */
layout(location = 2) uniform float samplePosMult; /* Multiply to push blur strength past the pixel offset */
layout(location = 3) uniform float pixelOffset; /* Pixel offset for this Kawase iteration */
layout(location = 4) uniform float bloomStrength; /* bloom strength */

in vec2 uv;

out vec4 fragmentColor;

void main() {
    /* Kawase blur samples 4 corners in a diamond pattern */
	vec2 o = vec2(pixelOffset + 0.5) * samplePosMult * frameSizeRCP;
	
	/* Sample the 4 diagonal corners with equal weight */
	vec4 color = vec4(0.0);
	color += texture2D(texture, uv + vec2( o.x,  o.y)); /* top-right */
	color += texture2D(texture, uv + vec2(-o.x,  o.y)); /* top-left   */
	color += texture2D(texture, uv + vec2(-o.x, -o.y)); /* bottom-left */
	color += texture2D(texture, uv + vec2( o.x, -o.y)); /* bottom-right */
	color /= 4.0;
	
	/* Apply bloom strength and output */
	fragmentColor.rgba = color * bloomStrength;
    //fragmentColor.a = 1.0;
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

    KawaseBlurShader& setFrameSize(Vector2i size) {
        Vector2 frameSizeRCP = {1.0f / static_cast<float>(size.x()), 1.0f / static_cast<float>(size.y())};
        setUniform(1, frameSizeRCP);
        return *this;
    }

    KawaseBlurShader& bind(GL::Texture2D& tex) {
        tex.bind(0);
        return *this;
    }

    KawaseBlurShader& bindBlurParams(float samplePosMult, float pixelOffset, float bloomStrength) {
        setUniform(2, samplePosMult);
        setUniform(3, pixelOffset);
        setUniform(4, bloomStrength);
        return *this;
    }

    MAGNUM_GL_ABSTRACTSHADERPROGRAM_SUBCLASS_DRAW_IMPLEMENTATION(KawaseBlurShader);
};

/**
 * Adds a bloom-like blur around emissive components on screen to fake the appearance of self-lighting.
 * TODO: expose params, maybe parallel blur stages per Kawase?
 */
class EmissiveEffect {
   public:
    EmissiveEffect() {
        const Vector2 posVertices[] = {{1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, -1.0f}, {-1.0f, 1.0f}};
        quadPosVertices.setData(posVertices, GL::BufferUsage::StaticDraw);

        const Vector2 texVertices[] = {{1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f}};
        quadTexVertices.setData(texVertices, GL::BufferUsage::StaticDraw);

        fullscreenQuad.setPrimitive(GL::MeshPrimitive::TriangleStrip)
            .setCount(4)
            .addVertexBuffer(quadPosVertices, 0, KawaseBlurShader::Position())
            .addVertexBuffer(quadTexVertices, 0, KawaseBlurShader::Texcoord());

        texColor1.setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setWrapping(GL::SamplerWrapping::ClampToEdge);

        texColor2.setMagnificationFilter(GL::SamplerFilter::Linear)
            .setMinificationFilter(GL::SamplerFilter::Linear, GL::SamplerMipmap::Linear)
            .setWrapping(GL::SamplerWrapping::ClampToEdge);
    }

    void setSize(Vector2i size) {
        if (size != framebuffer1.viewport().size()) {
            // HDR!
            texColor1.setStorage(1, GL::TextureFormat::RGBA16F, size);
            framebuffer1.setViewport({{}, size});
            texColor2.setStorage(1, GL::TextureFormat::RGBA16F, size);
            framebuffer2.setViewport({{}, size});
            texDepthAndStencil.setStorage(1, GL::TextureFormat::Depth24Stencil8, size);
            blurShader.setFrameSize(size);
        }
    }

    GL::AbstractFramebuffer& startDrawEmissive(GL::AbstractFramebuffer& outputFramebuffer) {
        using namespace Magnum::Math::Literals;
        using namespace Magnum::Math::Literals::ColorLiterals;
        setSize(outputFramebuffer.viewport().size());

        framebuffer1.bind();
        GL::Renderer::setDepthMask(false);
        GL::Renderer::enable(GL::Renderer::Feature::Blending);
        GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::SourceAlpha,
                                       GL::Renderer::BlendFunction::OneMinusSourceAlpha);

        framebuffer1.attachTexture(GL::Framebuffer::ColorAttachment(0), texColor1, 0);
        // framebuffer1.attachTexture(GL::Framebuffer::BufferAttachment::DepthStencil, texDepthAndStencil, 0);
        framebuffer1.mapForDraw({
            {Shaders::PhongGL::ColorOutput, GL::Framebuffer::ColorAttachment(0)},
        });
        framebuffer1.clearColor(0, 0x00000000_srgbaf);
        framebuffer1.clearDepth(1.0f);

        return framebuffer1;
    }

    void finishDrawEmissive(GL::AbstractFramebuffer& outputFramebuffer) {
        using namespace Magnum::Math::Literals::ColorLiterals;
        if (mEnabled) {
            // blur shader params
            // TODO: expose
            static int numBlurIterations = 3;  // at least 1
            static float samplePosMult = 1.2f;
            static float bloomStrength = 1.0f;

            ImGui::InputInt("numBlurIterations", &numBlurIterations);
            numBlurIterations = std::clamp(numBlurIterations, 1, 20);
            ImGui::DragFloat("samplePosMult", &samplePosMult);
            ImGui::DragFloat("bloomStrength", &bloomStrength);

            // kawase shader "ping-pong"
            auto* currentTex = &texColor1;
            auto* currentFb = &framebuffer1;
            auto* nextTex = &texColor2;
            auto* nextFb = &framebuffer2;
            for (int idx = 0; idx < numBlurIterations - 1; ++idx) {
                float pixelOffset = static_cast<float>(idx + 1);
                // blur only, doesn't need depth texture
                nextFb->bind();
                nextFb->attachTexture(GL::Framebuffer::ColorAttachment(0), *nextTex, 0);
                nextFb->mapForDraw({
                    {Shaders::PhongGL::ColorOutput, GL::Framebuffer::ColorAttachment(0)},
                });
                nextFb->clearColor(0, 0x00000000_srgbaf);
                blurShader.bind(*currentTex)
                    .bindBlurParams(samplePosMult, pixelOffset, bloomStrength)
                    .draw(fullscreenQuad);
                std::swap(currentTex, nextTex);
                std::swap(currentFb, nextFb);
            }

            // final draw to output
            outputFramebuffer.bind();
            float pixelOffset = static_cast<float>(numBlurIterations + 1);
            GL::Renderer::setBlendEquation(Magnum::GL::Renderer::BlendEquation::Add);
            GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::One, GL::Renderer::BlendFunction::One);
            blurShader.bind(*currentTex).bindBlurParams(samplePosMult, pixelOffset, bloomStrength).draw(fullscreenQuad);
        }

        GL::Renderer::setDepthMask(true);
        GL::Renderer::disable(GL::Renderer::Feature::Blending);
        GL::Renderer::setBlendFunction(GL::Renderer::BlendFunction::One, GL::Renderer::BlendFunction::Zero);
    }

   private:
    bool mEnabled = true;
    GL::Framebuffer framebuffer1{{{}, {}}};
    GL::Texture2D texColor1;
    GL::Framebuffer framebuffer2{{{}, {}}};
    GL::Texture2D texColor2;
    GL::Texture2D texDepthAndStencil;
    KawaseBlurShader blurShader;
    GL::Buffer quadPosVertices;
    GL::Buffer quadTexVertices;
    GL::Mesh fullscreenQuad;
};
}  // namespace kitgui::gfx