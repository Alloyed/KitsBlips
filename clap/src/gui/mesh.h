#pragma once

#include <GL/gl3w.h>
#include <tiny_gltf.h>
#include <string_view>

namespace kitgui {
class Model {
   public:
    void Load(std::string_view filename);
    void Bind();
    void Draw();

   private:
    tinygltf::Model mModel;
    std::pair<GLuint, std::map<int, GLuint>> mVaoAndEbos;
};
}  // namespace kitgui