#pragma once

#include <Magnum/GL/Mesh.h>
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
    uint32_t attributes;
    uint32_t vertices;
    uint32_t primitives;
    uint32_t objectIdCount;
    size_t size;
    std::string debugName;
    bool hasTangents, hasSeparateBitangents, hasVertexColors;
};

struct MeshCache {
    std::vector<MeshInfo> mMeshes;
    void LoadMeshes(Magnum::Trade::AbstractImporter& importer);
};
}  // namespace kitgui