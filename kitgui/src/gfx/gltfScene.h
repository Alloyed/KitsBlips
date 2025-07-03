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

   private:
    void Load(Magnum::Trade::AbstractImporter& importer, std::string_view debugName);

    std::unique_ptr<GltfSceneImpl> mImpl;
};
}  // namespace kitgui