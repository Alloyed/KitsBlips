#include "shaders.h"

#include <vector>
#include "battery/embed.hpp"

namespace kitgui {

ShaderProgram::ShaderProgram() {}

ShaderProgram::~ShaderProgram() {}

void ShaderProgram::Bind(const GladGLContext& gl) const {
    if (!mPid) {
        // Create the shaders
        GLuint VertexShaderID = gl.CreateShader(GL_VERTEX_SHADER);
        GLuint FragmentShaderID = gl.CreateShader(GL_FRAGMENT_SHADER);

        GLint Result = GL_FALSE;
        int InfoLogLength;

        // Compile Vertex Shader
        char const* VertexSourcePointer = b::embed<"assets/base.vert">().data();
        gl.ShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL);
        gl.CompileShader(VertexShaderID);

        // Check Vertex Shader
        gl.GetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
        gl.GetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
        if (InfoLogLength > 0) {
            std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
            gl.GetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
            printf("%s\n", &VertexShaderErrorMessage[0]);
        }

        // Compile Fragment Shader
        char const* FragmentSourcePointer = b::embed<"assets/base.frag">().data();
        gl.ShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL);
        gl.CompileShader(FragmentShaderID);

        // Check Fragment Shader
        gl.GetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
        gl.GetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
        if (InfoLogLength > 0) {
            std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
            gl.GetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
            printf("%s\n", &FragmentShaderErrorMessage[0]);
        }

        // Link the program
        printf("Linking program\n");
        GLuint ProgramID = gl.CreateProgram();
        gl.AttachShader(ProgramID, VertexShaderID);
        gl.AttachShader(ProgramID, FragmentShaderID);
        gl.LinkProgram(ProgramID);

        // Check the program
        gl.GetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
        gl.GetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
        if (InfoLogLength > 0) {
            std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
            gl.GetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
            printf("%s\n", &ProgramErrorMessage[0]);
        }

        gl.DetachShader(ProgramID, VertexShaderID);
        gl.DetachShader(ProgramID, FragmentShaderID);

        gl.DeleteShader(VertexShaderID);
        gl.DeleteShader(FragmentShaderID);

        // store important data
        mMVP = gl.GetUniformLocation(ProgramID, "MVP");
        mSunPosition = gl.GetUniformLocation(ProgramID, "sun_position");
        mSunColor = gl.GetUniformLocation(ProgramID, "sun_color");
        mPid = ProgramID;
    }
}

void ShaderProgram::Use(const GladGLContext& gl,
                        const glm::mat4& modelMatrix,
                        const glm::mat4& viewMatrix,
                        const glm::mat4& perspectiveMatrix,
                        const glm::vec3& sunPosition,
                        const glm::vec3& sunColor

) const {
    Bind(gl);

    gl.UseProgram(mPid);
    glm::mat4 mvp = perspectiveMatrix * viewMatrix * modelMatrix;

    gl.UniformMatrix4fv(mMVP, 1, GL_FALSE, &mvp[0][0]);
    gl.Uniform3fv(mSunPosition, 1, &sunPosition[0]);
    gl.Uniform3fv(mSunColor, 1, &sunColor[0]);
}

}  // namespace kitgui