#pragma once
#include <memory>
#include <string_view>

namespace Magnum::Trade {
class AbstractImporter;
}

namespace kitgui {
struct GltfSceneImpl;
class GltfScene {
   public:
    GltfScene();
    ~GltfScene();
    void Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);
    void Draw();

   private:
    std::unique_ptr<GltfSceneImpl> mImpl;
};
}  // namespace kitgui