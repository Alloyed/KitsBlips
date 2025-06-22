#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>

namespace kitgui {
class Shaders {
   public:
    Shaders();
    ~Shaders();
    void Use(const glm::mat4& modelMatrix,
             const glm::mat4& viewMatrix,
             const glm::mat4& perspectiveMatrix,
             const glm::vec3& sunPosition,
             const glm::vec3& sunColor) const;

   private:
    using UniformId = GLint;
    UniformId mMVP;
    UniformId mSunPosition;
    UniformId mSunColor;
    GLuint mPid;
};
}  // namespace kitgui