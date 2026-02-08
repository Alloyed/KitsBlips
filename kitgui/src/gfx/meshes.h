#pragma once

#include <Magnum/GL/Mesh.h>
#include <Magnum/Math/Range.h>
#include <optional>
#include <string>
#include <vector>

namespace Magnum {
namespace Trade {
class AbstractImporter;
}
}  // namespace Magnum

namespace kitgui {
struct MeshInfo {
    std::optional<Magnum::GL::Mesh> mesh;
    uint32_t id{};
    uint32_t attributes{};
    uint32_t vertices{};
    uint32_t primitives{};
    uint32_t objectIdCount{};
    size_t size{};
    std::string debugName{};
    Magnum::Range3D boundingBox{};
    bool hasTangents{};
    bool hasSeparateBitangents{};
    bool hasVertexColors{};

    void ImGui() const;
};

struct MeshCache {
    std::vector<MeshInfo> mMeshes;
    void LoadMeshes(Magnum::Trade::AbstractImporter& importer);
};
}  // namespace kitgui
