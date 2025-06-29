#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace kitgui {
class ShaderProgram {
   public:
    ShaderProgram();
    ~ShaderProgram();
    void Bind(const GladGLContext& gl) const;
    void Use(const GladGLContext& gl,
             const glm::mat4& modelMatrix,
             const glm::mat4& viewMatrix,
             const glm::mat4& perspectiveMatrix,
             const glm::vec3& sunPosition,
             const glm::vec3& sunColor) const;

   private:
    using UniformId = GLint;
    mutable UniformId mMVP;
    mutable UniformId mSunPosition;
    mutable UniformId mSunColor;
    mutable GLuint mPid = 0;
};
}  // namespace kitgui