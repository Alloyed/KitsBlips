#include "gfx/meshes.h"

// IWYU pragma: begin_keep
#include <Corrade/Containers/OptionalStl.h>
// IWYU pragma: end_keep

#include <Magnum/GL/Mesh.h>
#include <Magnum/MeshTools/Compile.h>
#include <Magnum/MeshTools/GenerateIndices.h>
#include <Magnum/MeshTools/BoundingVolume.h>
#include <Magnum/Trade/AbstractImporter.h>
#include <Magnum/Trade/MeshData.h>
#include <fmt/format.h>
#include <optional>
#include <string>
#include <vector>
#include <imgui.h>
#include "log.h"

using namespace Magnum;

namespace kitgui {
void MeshInfo::ImGui() const {
        if(ImGui::TreeNode(this, "Mesh: %s", debugName.c_str())) {
            ImGui::Text("objectIdCount: %d", objectIdCount);
            ImGui::Text("size: %ld", size);
            ImGui::Text("hasTangents: %s", hasTangents ? "true" : "false");
            ImGui::Text("hasSeparateBitangents: %s", hasSeparateBitangents ? "true" : "false");
            ImGui::Text("hasVertexColors: %s", hasVertexColors ? "true" : "false");
            ImGui::TreePop();
        }
}
void MeshCache::LoadMeshes(Magnum::Trade::AbstractImporter& importer) {
    /* Load all meshes. Meshes that fail to load will be NullOpt. Remember
       which have vertex colors, so in case there's no material we can use that
       instead. */
    kitgui::log::info(fmt::format("Loading {} meshes...", importer.meshCount()));
    mMeshes.clear();
    mMeshes.reserve(importer.meshCount());
    for (uint32_t i = 0; i != importer.meshCount(); ++i) {
        mMeshes.push_back({});
        MeshInfo& mesh = mMeshes.back();
        mesh.id = i;
        auto meshData = std::optional<Trade::MeshData>(importer.mesh(i));
        if (!meshData) {
            log::error(fmt::format("cannot load mesh {}: {} ", i, importer.meshName(i)));
            continue;
        }

        std::string meshName = importer.meshName(i);
        if (meshName.empty()) {
            meshName = fmt::format("{}", i);
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
                    log::info(fmt::format(
                        "Mesh {} doesn't have normals, generating smooth ones using information from the index buffer.",
                        meshName));
                    flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
                } else {
                    log::info(fmt::format("Mesh {} doesn't have normals, generating flat ones.", meshName));
                    flags |= MeshTools::CompileFlag::GenerateFlatNormals;
                }

                meshData = MeshTools::generateIndices(*Utility::move(meshData));

                /* Otherwise prefer smooth normals, if we have an index buffer
                   telling us neighboring faces */
            } else if (meshData->isIndexed()) {
                log::info(fmt::format(
                    "Mesh {} doesn't have normals, generating smooth ones using information from the index buffer.",
                    meshName));
                flags |= MeshTools::CompileFlag::GenerateSmoothNormals;
            } else {
                log::info(fmt::format("Mesh {} doesn't have normals, generating flat ones.", meshName));
                flags |= MeshTools::CompileFlag::GenerateFlatNormals;
            }
        }

        /* Print messages about ignored attributes / levels */
        for (uint32_t j = 0; j != meshData->attributeCount(); ++j) {
            const Trade::MeshAttribute attribute = meshData->attributeName(j);
            if (Trade::isMeshAttributeCustom(attribute)) {
                if (const std::string stringName = importer.meshAttributeName(attribute); !stringName.empty()) {
                    log::info(fmt::format("Mesh {} has a custom mesh attribute {}, ignoring", meshName, stringName));
                } else {
                    log::info(fmt::format("Mesh {} has a custom mesh attribute {}, ignoring", meshName, j));
                }
                continue;
            }

            const VertexFormat format = meshData->attributeFormat(j);
            if (isVertexFormatImplementationSpecific(format)) {
                if (const std::string attributeName = importer.meshAttributeName(attribute); !attributeName.empty()) {
                    log::info(fmt::format("Mesh attribute {}.{} has a custom mesh format {}, ignoring", meshName,
                                          attributeName, static_cast<uint32_t>(format)));
                } else {
                    log::info(fmt::format("Mesh attribute {}.{} has a custom mesh format {}, ignoring", meshName, j,
                                          static_cast<uint32_t>(format)));
                }
                continue;
            }
        }
        const uint32_t meshLevels = importer.meshLevelCount(i);
        if (meshLevels > 1) {
            log::info(
                fmt::format("Mesh attribute {} has {} additional mesh levels, ignoring", meshName, meshLevels - 1));
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
        mesh.boundingBox = Magnum::MeshTools::boundingRange(meshData->positions3DAsArray());
    }
}
}  // namespace kitgui
