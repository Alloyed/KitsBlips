#include "gfx/meshes.h"

#include <Corrade/Containers/BitArray.h>
#include <Corrade/Containers/OptionalStl.h>
#include <Corrade/Containers/Pair.h>
#include <Corrade/Containers/StridedArrayView.h>
#include <Corrade/Containers/Triple.h>
#include <Corrade/Utility/Algorithms.h>
#include <Magnum/GL/Mesh.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/AnimationData.h>
#include <Magnum/Trade/CameraData.h>
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Trade/PhongMaterialData.h>
#include <Magnum/Trade/SceneData.h>
#include <algorithm>
#include <cassert>
#include <format>
#include <optional>
#include <string>
#include <vector>

using namespace Magnum;

namespace kitgui {
void MeshCache::LoadMeshes(Magnum::Trade::AbstractImporter& importer) {
    /* Load all meshes. Meshes that fail to load will be NullOpt. Remember
       which have vertex colors, so in case there's no material we can use that
       instead. */
    Debug{} << "Loading" << importer.meshCount() << "meshes";
    mMeshes.clear();
    mMeshes.reserve(importer.meshCount());
    for (uint32_t i = 0; i != importer.meshCount(); ++i) {
        mMeshes.push_back({});
        MeshInfo& mesh = mMeshes.back();
        auto meshData = std::optional<Trade::MeshData>(importer.mesh(i));
        if (!meshData) {
            Warning{} << "Cannot load mesh" << i << importer.meshName(i);
            continue;
        }

        std::string meshName = importer.meshName(i);
        if (meshName.empty()) {
            meshName = std::format("{}", i);
        }

        /* Disable warnings on custom attributes, as we printed them with
           actual string names below. Generate normals for triangle meshes
           (and don't do anything for line/point meshes, there it makes no
           sense). */
        MeshTools::CompileFlags flags = MeshTools::CompileFlag::NoWarnOnCustomAttributes;
        if ((meshData->primitive() == MeshPrimitive::Triangles ||
             meshData->primitive() == MeshPrimitive::TriangleStrip ||
             meshData->primitive() == MeshPrimitive::TriangleFan) &&
            !meshData->attributeCount(Trade::MeshAttribute::Normal) &&
            meshData->hasAttribute(Trade::MeshAttribute::Position) &&
            meshData->attributeFormat(Trade::MeshAttribute::Position) == VertexFormat::Vector3) {
            /* If the mesh is a triangle strip/fan, convert to indexed
               triangles first. If the strip/fan is indexed, we can attempt to
               generate smooth normals. If it's not, generate flat ones as
               otherwise the smoothing would be only along the strip and not at
               the seams, looking weird. */
            if (meshData->primitive() == MeshPrimitive::TriangleStrip ||
                meshData->primitive() == MeshPrimitive::TriangleFan) {
                if (meshData->isIndexed()) {
                    Debug{} << "Mesh" << meshName.c_str()
                            << "doesn't have normals, generating smooth ones using information from the index "
                               "buffer for a"
                            << meshData->primitive();
                    flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
                } else {
                    Debug{} << "Mesh" << meshName.c_str() << "doesn't have normals, generating flat ones for a"
                            << meshData->primitive();
                    flags |= MeshTools::CompileFlag::GenerateFlatNormals;
                }

                meshData = MeshTools::generateIndices(*Utility::move(meshData));

                /* Otherwise prefer smooth normals, if we have an index buffer
                   telling us neighboring faces */
            } else if (meshData->isIndexed()) {
                Debug{} << "Mesh" << meshName.c_str()
                        << "doesn't have normals, generating smooth ones using information from the index buffer";
                flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
            } else {
                Debug{} << "Mesh" << meshName.c_str() << "doesn't have normals, generating flat ones";
                flags |= MeshTools::CompileFlag::GenerateFlatNormals;
            }
        }

        /* Print messages about ignored attributes / levels */
        for (uint32_t j = 0; j != meshData->attributeCount(); ++j) {
            const Trade::MeshAttribute name = meshData->attributeName(j);
            if (Trade::isMeshAttributeCustom(name)) {
                if (const std::string stringName = importer.meshAttributeName(name); !stringName.empty()) {
                    Warning{} << "Mesh" << meshName.c_str() << "has a custom mesh attribute" << stringName.c_str()
                              << Debug::nospace << ", ignoring";
                } else {
                    Warning{} << "Mesh" << meshName.c_str() << "has a custom mesh attribute" << name << Debug::nospace
                              << ", ignoring";
                }
                continue;
            }

            const VertexFormat format = meshData->attributeFormat(j);
            if (isVertexFormatImplementationSpecific(format)) {
                Warning{} << "Mesh" << meshName.c_str() << "has" << name << "of format" << format << Debug::nospace
                          << ", ignoring";
            }
        }
        const uint32_t meshLevels = importer.meshLevelCount(i);
        if (meshLevels > 1) {
            Warning{} << "Mesh" << meshName.c_str() << "has" << meshLevels - 1 << "additional mesh levels, ignoring";
        }

        /* Save metadata, compile the mesh */
        mesh.hasVertexColors = meshData->hasAttribute(Trade::MeshAttribute::Color);
        mesh.attributes = meshData->attributeCount();
        mesh.vertices = meshData->vertexCount();
        mesh.size = meshData->vertexData().size();
        if (meshData->isIndexed()) {
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->indexCount());
            mesh.size += meshData->indexData().size();
        } else {
            mesh.primitives = MeshTools::primitiveCount(meshData->primitive(), meshData->vertexCount());
        }
        /* Needed for a warning when using a mesh with no tangents with a
           normal map (as, unlike with normals, we have no builtin way to
           generate tangents right now) */
        mesh.hasTangents = meshData->hasAttribute(Trade::MeshAttribute::Tangent);
        /* Needed to decide how to visualize tangent space */
        mesh.hasSeparateBitangents = meshData->hasAttribute(Trade::MeshAttribute::Bitangent);

        mesh.objectIdCount = 0;
        if (meshData->hasAttribute(Trade::MeshAttribute::ObjectId)) {
            for (const auto id : meshData->objectIdsAsArray()) {
                mesh.objectIdCount = std::max(mesh.objectIdCount, id);
            }
        }
        mesh.mesh = MeshTools::compile(*meshData, flags);
        mesh.debugName = meshName;
    }
}
}  // namespace kitgui