//
// Created by jglrxavpok on 02/08/2023.
//

#include "NavMeshBuilder.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <core/math/AABB.h>
#include <core/math/Sphere.h>
#include <tribox3.h>
#include <core/scene/LoadedScene.h>
#include <engine/render/resources/model_loading/SceneLoader.h>
#include <core/io/Logging.hpp>

namespace Carrot::AI {

    void NavMeshBuilder::start(std::vector<MeshEntry>&& _entries, float voxelSize, float maxSlope) {
        verify(!isRunning(), "Cannot start a NavMeshBuilder which is still running");

        entries = std::move(_entries);
        GetTaskScheduler().schedule(TaskDescription {
            .name = "Build NavMesh",
            .task = build(voxelSize, maxSlope),
            .joiner = &taskRunning
        }, TaskScheduler::AssetLoading);
    }

    bool NavMeshBuilder::isRunning() {
        return !taskRunning.isIdle();
    }

    void NavMeshBuilder::debugDraw(const Carrot::Render::Context& renderContext) {
        const glm::vec3 halfExtents {0.5f * voxelSize};
        const glm::vec4 walkableColor = glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f };
        const glm::vec4 nonWalkableColor = glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f };
        for(const auto& [position, voxel] : voxels) {
            const glm::mat4 transform = glm::translate(glm::mat4{1.0f}, minVoxelPosition + glm::vec3 { position } * voxelSize + halfExtents);
            GetRenderer().renderWireframeCuboid(renderContext, transform, halfExtents, voxel.walkable ? walkableColor : nonWalkableColor);
        }
        const glm::mat4 transform = glm::translate(glm::mat4{1.0f}, minVoxelPosition + size/2.0f);
        GetRenderer().renderWireframeCuboid(renderContext, transform, size/2.0f, walkableColor);
    }

    Carrot::Async::Task<void> NavMeshBuilder::build(float _voxelSize, float maxSlope) {
        voxelSize = _voxelSize;
        debugStep = "Compute bounds";

        Math::AABB completeBounds;
        completeBounds.min = glm::vec3{ +INFINITY };
        completeBounds.max = glm::vec3{ -INFINITY };
        for(std::size_t i = 0; i < entries.size(); i++) {
            auto pModel = entries[i].model;
            auto modelTransform = entries[i].transform;
            for(const auto& [_, meshes] : pModel->getStaticMeshesPerMaterial()) {
                for(const auto& meshInfo : meshes) {
                    Math::Sphere worldSphere = meshInfo.boundingSphere;
                    worldSphere.transform(modelTransform * meshInfo.transform);

                    Math::AABB bounds;
                    bounds.loadFromSphere(worldSphere);

                    completeBounds.min = glm::min(completeBounds.min, bounds.min);
                    completeBounds.max = glm::max(completeBounds.max, bounds.max);
                }
            }
        }

        debugStep = "Allocate memory for voxelisation";

        minVoxelPosition = completeBounds.min;
        size = completeBounds.max - completeBounds.min;

        sizeX = ceil(size.x / voxelSize);
        sizeY = ceil(size.y / voxelSize);
        sizeZ = ceil(size.z / voxelSize);
        voxels.reset(sizeX, sizeY, sizeZ);

        const glm::vec3 halfSize { voxelSize / 2.0f };
        const glm::vec3 upVector { 0.0f, 0.0f, 1.0f };

        Math::AABB triangleBounds;
        float vertices[3][3];
        // TODO: //-ize?
        for(std::size_t i = 0; i < entries.size(); i++) {
            auto pModel = entries[i].model;
            debugStep = Carrot::sprintf("Voxelise meshes %llu / %llu - %s", i, entries.size(), pModel->getOriginatingResource().getName().c_str());
            // reload original model to have CPU visible meshes. We could copy from GPU but that would be painful to write

            Render::SceneLoader loader;
            Render::LoadedScene& scene = loader.load(pModel->getOriginatingResource());

            std::function<void(const Carrot::Render::SkeletonTreeNode&, glm::mat4)> recursivelyLoadNodes = [&](const Carrot::Render::SkeletonTreeNode& node, const glm::mat4& nodeTransform) {
                glm::mat4 transform = nodeTransform * node.bone.originalTransform;
                auto& potentialMeshes = node.meshIndices;
                if(potentialMeshes.has_value()) {
                    const glm::mat3 normalTransform = glm::transpose(glm::inverse(glm::mat3{transform}));
                    for(const auto& meshIndex : potentialMeshes.value()) {
                        // for each primitive
                        auto& primitive = scene.primitives[meshIndex];

                        // for each triangle, intersect all voxels the triangle spans over, changing their state if there is an intersection
                        std::int64_t j = 0;
                        for(; j < primitive.indices.size(); j += 3) {
                            triangleBounds.min = glm::vec3{ +INFINITY };
                            triangleBounds.max = glm::vec3{ -INFINITY };

                            glm::vec3 normal{0.0f};
                            for (int vertexInTriangle = 0; vertexInTriangle < 3; ++vertexInTriangle) {
                                const std::uint32_t index = primitive.indices[j + vertexInTriangle];
                                glm::vec4 vertexPosition = transform * primitive.vertices[index].pos;
                                vertexPosition.x /= vertexPosition.w;
                                vertexPosition.y /= vertexPosition.w;
                                vertexPosition.z /= vertexPosition.w;
                                for (int dimension = 0; dimension < 3; ++dimension) {
                                    vertices[vertexInTriangle][dimension] = vertexPosition[dimension];
                                    triangleBounds.min[dimension] = glm::min(triangleBounds.min[dimension], vertexPosition[dimension]);
                                    triangleBounds.max[dimension] = glm::max(triangleBounds.max[dimension], vertexPosition[dimension]);
                                }
                                normal += normalTransform * primitive.vertices[index].normal;
                            }
                            normal = glm::normalize(normal);

                            const bool walkable = glm::angle(normal, upVector) <= maxSlope;

                            triangleBounds.min = glm::floor(triangleBounds.min / voxelSize) * voxelSize - voxelSize;
                            triangleBounds.max = glm::ceil(triangleBounds.max / voxelSize) * voxelSize + voxelSize;
                            for(float z = triangleBounds.min.z; z <= triangleBounds.max.z; z += voxelSize) {
                                for(float y = triangleBounds.min.y; y <= triangleBounds.max.y; y += voxelSize) {
                                    for (float x = triangleBounds.min.x; x <= triangleBounds.max.x; x += voxelSize) {
                                        const glm::vec3 boxCorner {x, y, z};
                                        const glm::vec3 boxCenter = boxCorner + halfSize;

                                        float c[3] = { x, y, z };
                                        float h[3] = { halfSize.x, halfSize.y, halfSize.z };
                                        bool intersect = triBoxOverlap(c, h, vertices) != 0;

                                        if(intersect) {
                                            const glm::ivec3 localPosition = glm::floor((boxCorner - completeBounds.min + halfSize) / voxelSize);
                                            if(localPosition.x >= 0 && localPosition.x < sizeX
                                               && localPosition.y >= 0 && localPosition.y < sizeY
                                               && localPosition.z >= 0 && localPosition.z < sizeZ) {
                                                auto& voxel = voxels.insert(localPosition);
                                                voxel.walkable &= walkable;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        Carrot::Log::info("index = %lld / %llu", j, primitive.indices.size());
                    }
                }

                for(auto& child : node.getChildren()) {
                    recursivelyLoadNodes(child, transform);
                }
            };

            recursivelyLoadNodes(scene.nodeHierarchy->hierarchy, entries[i].transform);
        }
        voxels.finishBuild();

        // TODO: voxelisation is done, now compute:
        //  1. clearance (height & width gaps)
        //  2. create mesh based on connections
        //  3. ???

        debugStep = "Finished!";
        co_return;
    }
} // Carrot::AI