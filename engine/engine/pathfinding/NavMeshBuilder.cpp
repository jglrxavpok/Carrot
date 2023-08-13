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

    static std::array<glm::vec4, 45> regionColors {
            glm::vec4
            {189.0f / 255.0f, 236.0f / 255.0f, 182.0f / 255.0f, 1.0f},
            {108.0f / 255.0f, 112.0f / 255.0f,  89.0f / 255.0f, 1.0f},
            {203.0f / 255.0f, 208.0f / 255.0f, 204.0f / 255.0f, 1.0f},
            {250.0f / 255.0f, 210.0f / 255.0f,  01.0f / 255.0f, 1.0f},
            {220.0f / 255.0f, 156.0f / 255.0f,   0.0f / 255.0f, 1.0f},
            { 42.0f / 255.0f, 100.0f / 255.0f, 120.0f / 255.0f, 1.0f},
            {120.0f / 255.0f, 133.0f / 255.0f, 139.0f / 255.0f, 1.0f},
            {121.0f / 255.0f,  85.0f / 255.0f,  61.0f / 255.0f, 1.0f},
            {157.0f / 255.0f, 145.0f / 255.0f,  01.0f / 255.0f, 1.0f},
            {166.0f / 255.0f,  94.0f / 255.0f,  46.0f / 255.0f, 1.0f},
            {203.0f / 255.0f,  40.0f / 255.0f,  33.0f / 255.0f, 1.0f},
            {243.0f / 255.0f, 159.0f / 255.0f,  24.0f / 255.0f, 1.0f},
            {250.0f / 255.0f, 210.0f / 255.0f,  01.0f / 255.0f, 1.0f},
            {114.0f / 255.0f,  20.0f / 255.0f,  34.0f / 255.0f, 1.0f},
            { 64.0f / 255.0f,  58.0f / 255.0f,  58.0f / 255.0f, 1.0f},
            {157.0f / 255.0f, 161.0f / 255.0f, 170.0f / 255.0f, 1.0f},
            {164.0f / 255.0f, 125.0f / 255.0f, 144.0f / 255.0f, 1.0f},
            {248.0f / 255.0f,   0.0f / 255.0f,   0.0f / 255.0f, 1.0f},
            {120.0f / 255.0f,  31.0f / 255.0f,  25.0f / 255.0f, 1.0f},
            { 51.0f / 255.0f,  47.0f / 255.0f,  44.0f / 255.0f, 1.0f},
            {180.0f / 255.0f,  76.0f / 255.0f,  67.0f / 255.0f, 1.0f},
            {125.0f / 255.0f, 132.0f / 255.0f, 113.0f / 255.0f, 1.0f},
            {161.0f / 255.0f,  35.0f / 255.0f,  18.0f / 255.0f, 1.0f},
            {142.0f / 255.0f,  64.0f / 255.0f,  42.0f / 255.0f, 1.0f},
            {130.0f / 255.0f, 137.0f / 255.0f, 143.0f / 255.0f, 1.0f},
    };


    void NavMeshBuilder::start(std::vector<MeshEntry>&& _entries, const BuildParams& buildParams) {
        verify(!isRunning(), "Cannot start a NavMeshBuilder which is still running");
        params = buildParams;

        entries = std::move(_entries);
        GetTaskScheduler().schedule(TaskDescription {
            .name = "Build NavMesh",
            .task = build(),
            .joiner = &taskRunning
        }, TaskScheduler::AssetLoading);
    }

    bool NavMeshBuilder::isRunning() {
        return !taskRunning.isIdle();
    }

    void NavMeshBuilder::debugDraw(const Carrot::Render::Context& renderContext, DebugDrawType drawType) {
        const glm::vec3 halfExtents {0.5f * params.voxelSize};
        const glm::vec4 walkableColor = glm::vec4{ 0.0f, 0.0f, 1.0f, 1.0f };
        const glm::vec4 nonWalkableColor = glm::vec4{ 1.0f, 0.0f, 0.0f, 1.0f };
        if(drawType == DebugDrawType::WalkableVoxels) {
            for(const auto& [position, voxel] : workingData.walkableVoxels) {
                const glm::mat4 transform = glm::translate(glm::mat4{1.0f}, minVoxelPosition + glm::vec3 { position } * params.voxelSize + halfExtents);
                GetRenderer().renderCuboid(renderContext, transform, halfExtents, voxel.walkable ? walkableColor : nonWalkableColor);
            }
        } else if(drawType == DebugDrawType::OpenHeightField || drawType == DebugDrawType::DistanceField || drawType == DebugDrawType::Regions) {
            const auto& field = workingData.openHeightField;
            for(std::int64_t y = 0; y < sizeY; y++) { // this is stupidly parallel, MAY benefit from //-ization if needed
                for(std::int64_t x = 0; x < sizeX; x++) {
                    const std::size_t columnIndex = x + y * sizeX;
                    if(!field.contains(columnIndex)) { // column full of non walkable space
                        continue;
                    }

                    const auto& column = field.at(columnIndex);
                    if(column.spans.empty()) {
                        continue;
                    }

                    for(const auto& span : column.spans) {
                        const glm::mat4 transform = glm::translate(glm::mat4{1.0f}, minVoxelPosition + glm::vec3 { x, y, span.bottomZ } * params.voxelSize + halfExtents);

                        glm::vec4 color = glm::vec4(0,1,0,1);
                        if(drawType == DebugDrawType::DistanceField) {
                            color = glm::vec4(glm::vec3{(float)span.distanceToBorder / workingData.maxDistance}, 1.0f);
                        } else if(drawType == DebugDrawType::Regions) {
                            if(span.regionID <= 0) {
                                continue;
                            }
                            color = regionColors[(span.regionID - 1) % regionColors.size()];
                        }

                        GetRenderer().renderCuboid(renderContext, transform, glm::vec3 { halfExtents.x, halfExtents.y, halfExtents.z / 5.0f }, color);
                    }
                }
            }

            for(const auto& region : workingData.regions) {

                const std::size_t columnIndex = region.center.x + region.center.y * sizeX;
                const auto& centerSpan = workingData.openHeightField.at(columnIndex).spans[region.center.z];
                if(centerSpan.regionID <= 0) {
                    continue;
                }
                const glm::vec4 color = glm::vec4(1.0f);
                const glm::vec3 position = minVoxelPosition + glm::vec3 { region.center.x, region.center.y, centerSpan.bottomZ } * params.voxelSize + halfExtents;
                const glm::mat4 transform = glm::translate(glm::mat4{1.0f}, position);
                GetRenderer().render3DArrow(renderContext, transform, color);
            }
        } else if(drawType == DebugDrawType::Contours) {
            for(const auto& region : workingData.regions) {
                for(const auto& contourPoint : region.contour) {
                    const std::size_t columnIndex = contourPoint.x + contourPoint.y * sizeX;
                    const auto& centerSpan = workingData.openHeightField.at(columnIndex).spans[contourPoint.z];
                    if(centerSpan.regionID <= 0) {
                        continue;
                    }
                    const glm::vec4 color = regionColors[region.index % regionColors.size()];
                    const glm::vec3 position = minVoxelPosition + glm::vec3 { contourPoint.x, contourPoint.y, centerSpan.bottomZ } * params.voxelSize + halfExtents;
                    const glm::mat4 transform = glm::translate(glm::mat4{1.0f}, position);
                    GetRenderer().render3DArrow(renderContext, transform, color);
                }
            }
        }
    }

    Carrot::Async::Task<void> NavMeshBuilder::build() {
        // 1. voxelise given meshes
        // 2. remove voxels based on clearance & XY gap (min corridor size)
        // 3. generate mesh

        const float voxelSize = params.voxelSize;
        const float maxSlope = params.maxSlope;
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
        workingData = {};

        minVoxelPosition = completeBounds.min;
        size = completeBounds.max - completeBounds.min;

        sizeX = ceil(size.x / voxelSize);
        sizeY = ceil(size.y / voxelSize);
        sizeZ = ceil(size.z / voxelSize);

        auto& voxels = workingData.voxelizedScene;
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
                                                voxel.walkable |= walkable;
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

        debugStep = "Remove voxels with unsufficient clearance";

        workingData.walkableVoxels.reset(sizeX, sizeY, sizeZ);
        for(const auto& [position, voxel] : voxels) {
            if(voxel.walkable) {
                workingData.walkableVoxels.insert(position);
            }
        }
        workingData.walkableVoxels.finishBuild();

        // 1. open heightfield, handle climbable steps & connectivity here
        buildOpenHeightField(workingData.voxelizedScene, workingData.openHeightField);

        // 2. from connectivity & heightfield, compute distance field
        buildDistanceField(workingData.openHeightField);

        // 3. from distance field, remove cells where agents cannot walk (too narrow)
        // TODO: narrowDistanceField(workingData.openHeightField);

        // 4. from distance field, create regions (watershed ??) and determine region connectivity (walk along contour and find connected regions)
        buildRegions(workingData.openHeightField, workingData.regions);

        // 5. create contours
        buildContours(workingData.openHeightField, workingData.regions);

        // 6. from contours, create meshes & triangulate them (reuse vertices between regions to keep connectivity?)
        // TODO

        debugStep = "Finished!";
        co_return;
    }

    bool NavMeshBuilder::doSpansConnect(const HeightFieldSpan& spanA, const HeightFieldSpan& spanB) {
        return abs(spanA.bottomZ - spanB.bottomZ) <= params.maxClimbHeight;
    }

    void NavMeshBuilder::buildOpenHeightField(const SparseVoxelGrid& voxels, OpenHeightField& field) {
        field.resize(sizeX * sizeY);

        debugStep = "Open heightfield generation";
        // for each column, find spans of open space along Z axis
        for(std::size_t y = 0; y < sizeY; y++) {
            for(std::size_t x = 0; x < sizeX; x++) {
                HeightFieldSpan* span = nullptr;
                const std::size_t columnIndex = x + y * sizeX;

                bool emptySpace = false;
                for(std::int64_t z = 0; z < sizeZ; z++) {
                    if(voxels.contains(x, y, z)) {
                        const Voxel& voxel = voxels.get(x, y, z);
                        if(voxel.walkable) {
                            if(emptySpace || span == nullptr) { // new column starting here
                                span = &field[columnIndex].spans.emplace_back();
                                span->bottomZ = z;
                                span->height = 1;
                                emptySpace = false;
                            } else { // continuing column below this voxel
                                span->height++;
                            }
                        } else { // stop this column, if any
                            span = nullptr;
                        }
                    } else {
                        emptySpace = true;
                        // empty space, continue the column, if any
                        if(span != nullptr) {
                            span->height++;
                        }
                    }
                }

                if(span != nullptr) {
                    span->height = sizeZ-span->bottomZ; // will reach the ceiling
                }
            }
        }

        debugStep = "Remove small gaps";
        for(std::int64_t y = 0; y < sizeY; y++) { // this is stupidly parallel, MAY benefit from //-ization if needed
            for (std::int64_t x = 0; x < sizeX; x++) {
                const std::size_t columnIndex = x + y * sizeX;
                if (!field.contains(columnIndex)) { // column full of non walkable space
                    continue;
                }

                auto& column = field[columnIndex];

                std::erase_if(column.spans, [&](const HeightFieldSpan& span) {
                    return span.height < params.characterHeight && span.bottomZ + span.height < sizeZ /* if we reach the ceiling, the span is still walkable */;
                });
            }
        }

        debugStep = "Open heightfield adjacency";
        // connect adjacent spans (based on step height)
        for(std::int64_t y = 0; y < sizeY; y++) { // this is stupidly parallel, MAY benefit from //-ization if needed
            for(std::int64_t x = 0; x < sizeX; x++) {
                const std::size_t columnIndex = x + y * sizeX;
                if(!field.contains(columnIndex)) { // column full of non walkable space
                    continue;
                }

                auto& column = field[columnIndex];
                if(column.spans.empty()) {
                    continue;
                }

                // check connections in each direction
                for(std::uint8_t dir = 0; dir < 4; dir++) {
                    const std::int64_t nextX = Dx[dir] + x;
                    const std::int64_t nextY = Dy[dir] + y;

                    if(nextX < 0 || nextY < 0 || nextX >= sizeX || nextY >= sizeY) { // out-of-bounds
                        continue;
                    }

                    const std::size_t adjacentColumn = nextX + nextY * sizeX;
                    if(!field.contains(adjacentColumn)) { // no walkable space in that direction
                        continue;
                    }

                    const auto& otherColumn = field[adjacentColumn];

                    // check each span of this column against spans of the other column
                    // TODO: due to build order, spans are sorted, maybe we don't need to iterate over all spans?
                    for(auto& span : column.spans) {
                        for(auto& otherSpan : otherColumn.spans) {
                            if(doSpansConnect(span, otherSpan)) {
                                span.connected[dir] = true;
                            }
                        }
                    }
                }
            }
        }
    }

    void NavMeshBuilder::buildDistanceField(OpenHeightField& field) {
        debugStep = "Build distance field init";
        // initialize
        for (std::int64_t y = 0; y < sizeY; y++) { // this is stupidly parallel, MAY benefit from //-ization if needed
            for (std::int64_t x = 0; x < sizeX; x++) {
                const std::size_t columnIndex = x + y * sizeX;
                if (!field.contains(columnIndex)) { // column full of non walkable space
                    continue;
                }

                auto& column = field[columnIndex];
                for (auto& span: column.spans) {
                    std::size_t connectionCount = 0;

                    for (int i = 0; i < DirectionCount; i++) {
                        if (span.connected[i]) {
                            connectionCount++;
                        }
                    }

                    if (connectionCount != 4) { // border
                        span.distanceToBorder = 0;
                    } else { // inside
                        span.distanceToBorder = std::numeric_limits<std::int64_t>::max();
                    }
                }
            }
        }

        debugStep = "Build distance field pass 1";
        // pass 1
        for (std::int64_t y = 1; y < sizeY; y++) { // this is stupidly parallel, MAY benefit from //-ization if needed
            for (std::int64_t x = 1; x < sizeX; x++) {
                const std::size_t columnIndex = x + y * sizeX;
                if (!field.contains(columnIndex)) { // column full of non walkable space
                    continue;
                }

                auto& column = field[columnIndex];

                for (int dir: {Left, Backwards}) {
                    const std::int64_t nextX = Dx[dir] + x;
                    const std::int64_t nextY = Dy[dir] + y;

                    assert(nextX >= 0 && nextY >= 0 && nextX < sizeX && nextY < sizeY); // check if in-bounds

                    const std::size_t adjacentColumn = nextX + nextY * sizeX;
                    if (!field.contains(adjacentColumn)) { // no walkable space in that direction
                        continue;
                    }

                    const auto& otherColumn = field[adjacentColumn];

                    // check each span of this column against spans of the other column
                    // TODO: due to build order, spans are sorted, maybe we don't need to iterate over all spans?

                    for (auto& span: column.spans) {
                        std::int64_t newDistance = span.distanceToBorder;
                        for (auto& otherSpan: otherColumn.spans) {
                            if (doSpansConnect(span, otherSpan)) {
                                newDistance = std::min(newDistance, otherSpan.distanceToBorder+1);
                            }
                        }
                        span.distanceToBorder = newDistance;
                    }
                }
            }
        }

        debugStep = "Build distance field pass 2";
        // pass 2
        for (std::int64_t y = sizeY - 1; y >= 0; y--) { // this is stupidly parallel, MAY benefit from //-ization if needed
            for (std::int64_t x = sizeX - 1; x >= 0; x--) {
                const std::size_t columnIndex = x + y * sizeX;
                if (!field.contains(columnIndex)) { // column full of non walkable space
                    continue;
                }

                auto& column = field[columnIndex];

                for (int dir: {Right, Forward}) {
                    const std::int64_t nextX = Dx[dir] + x;
                    const std::int64_t nextY = Dy[dir] + y;

                    assert(nextX >= 0 && nextY >= 0 && nextX < sizeX && nextY < sizeY); // check if in-bounds

                    const std::size_t adjacentColumn = nextX + nextY * sizeX;
                    if (!field.contains(adjacentColumn)) { // no walkable space in that direction
                        continue;
                    }

                    const auto& otherColumn = field[adjacentColumn];

                    // check each span of this column against spans of the other column
                    // TODO: due to build order, spans are sorted, maybe we don't need to iterate over all spans?

                    for (auto& span: column.spans) {
                        std::int64_t newDistance = span.distanceToBorder;
                        for (auto& otherSpan: otherColumn.spans) {
                            if (doSpansConnect(span, otherSpan)) {
                                newDistance = std::min(newDistance, otherSpan.distanceToBorder+1);
                            }
                        }
                        span.distanceToBorder = newDistance;
                    }
                }
            }
        }

        debugStep = "Build distance field compute max";
        // compute max
        workingData.maxDistance = 0;
        for (std::int64_t y = 0; y < sizeY; y++) {
            for (std::int64_t x = 0; x < sizeX; x++) {
                const std::size_t columnIndex = x + y * sizeX;
                if (!field.contains(columnIndex)) { // column full of non walkable space
                    continue;
                }

                auto& column = field[columnIndex];
                for (auto& span: column.spans) {
                    workingData.maxDistance = std::max(workingData.maxDistance, span.distanceToBorder);
                }
            }
        }
    }

    void NavMeshBuilder::floodFill(OpenHeightField& field, const Region& region) {
        const std::size_t baseColumnIndex = region.center.x + region.center.y * sizeX;
        auto& baseSpan = field.at(baseColumnIndex).spans[region.center.z];
        const std::int64_t distance = baseSpan.distanceToBorder;

        std::stack<glm::ivec3> toProcess; // Z is span index
        toProcess.push(region.center);

        while(!toProcess.empty()) {
            const glm::ivec3 spanPosition = toProcess.top();
            toProcess.pop();

            const std::size_t columnIndex = spanPosition.x + spanPosition.y * sizeX;
            auto& span = field.at(columnIndex).spans[spanPosition.z];
            if(span.distanceToBorder == distance && span.regionID == 0) {
                span.regionID = region.index+1;

                for(int dir = 0; dir < DirectionCount; dir++) {
                    if(!span.connected[dir]) {
                        continue;
                    }

                    const std::int64_t nextX = Dx[dir] + spanPosition.x;
                    const std::int64_t nextY = Dy[dir] + spanPosition.y;

                    if(nextX < 0 || nextY < 0 || nextX >= sizeX || nextY >= sizeY) { // check if in-bounds
                        continue;
                    }

                    const std::size_t adjacentColumn = nextX + nextY * sizeX;
                    if (!field.contains(adjacentColumn)) { // no walkable space in that direction
                        continue;
                    }

                    const auto& otherColumn = field[adjacentColumn];
                    for(std::size_t otherSpanIndex = 0; otherSpanIndex < otherColumn.spans.size(); otherSpanIndex++) {
                        if(doSpansConnect(span, otherColumn.spans[otherSpanIndex])) {
                            toProcess.emplace(nextX, nextY, otherSpanIndex);
                        }
                    }
                }
            }
        }
    }

    void NavMeshBuilder::buildRegions(OpenHeightField& field, std::vector<Region>& regions) {
        debugStep = "Build regions";
        // distance field is considered as an inverted heightmap, and we fill craters with water progressively
        // maybe this is like the watershed algorithm? Don't know, can't access the original paper anyway

        using SortedSpans = std::vector<glm::ivec3>; // span coords = { X, Y, Index of span inside column }
        std::vector<SortedSpans> sortedSpans; // sort spans by their distance to the border, one entry per distance value
        sortedSpans.resize(workingData.maxDistance+1);

        for (std::int64_t y = 0; y < sizeY; y++) {
            for (std::int64_t x = 0; x < sizeX; x++) {
                const std::size_t columnIndex = x + y * sizeX;
                if (!field.contains(columnIndex)) { // column full of non walkable space
                    continue;
                }

                auto& column = field[columnIndex];
                for (std::size_t i = 0; i < column.spans.size(); i++) {
                    const auto& span = column.spans[i];

                    sortedSpans[span.distanceToBorder].emplace_back(x, y, i);
                }
            }
        }

        for(std::int64_t depth = workingData.maxDistance; depth >= 0; depth--) {
            auto& spanCoords = sortedSpans[depth];

            // do it twice to handle corners
            for(int iter = 0; iter < 2; iter++) {
                // for each span
                for(const auto& coords : spanCoords) {
                    const std::size_t columnIndex = coords.x + coords.y * sizeX;
                    auto& span = field.at(columnIndex).spans[coords.z];
                    if(span.regionID != 0) {
                        continue;
                    }

                    bool foundRegion = false;
                    // check for each direction if there is a connected neighbor

                    for(int dir = 0; dir < DirectionCount; dir++) {
                        if(foundRegion) {
                            break;
                        }
                        if(!span.connected[dir]) {
                            continue;
                        }
                        const std::int64_t nextX = Dx[dir] + coords.x;
                        const std::int64_t nextY = Dy[dir] + coords.y;

                        if(nextX < 0 || nextY < 0 || nextX >= sizeX || nextY >= sizeY) { // check if in-bounds
                            continue;
                        }

                        const std::size_t adjacentColumn = nextX + nextY * sizeX;
                        if (!field.contains(adjacentColumn)) { // no walkable space in that direction
                            continue;
                        }

                        const auto& otherColumn = field[adjacentColumn];
                        for(const auto& other : otherColumn.spans) {
                            // if there is a connected neighbor,
                            if(doSpansConnect(span, other)) {
                                // check if it has a region, if yes -> connect to the neighbor's region
                                if(other.regionID > 0) {
                                    span.regionID = other.regionID;

                                    // found a region, no need to go further than that
                                    foundRegion = true;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            for(const auto& coords : spanCoords) {
                const std::size_t columnIndex = coords.x + coords.y * sizeX;
                auto& span = field.at(columnIndex).spans[coords.z];
                if (span.regionID != 0) {
                    continue;
                }

                // new region!
                // note: this can affect the spans we check next in the iteration (if they are neighbors), this is on purpose to avoid creating tiny regions next to one another
                auto& newRegion = regions.emplace_back();
                newRegion.center = coords;
                newRegion.index = regions.size() - 1;

                floodFill(field, newRegion);
            }
        }
    }

    void NavMeshBuilder::buildContours(const OpenHeightField& field, std::vector<Region>& regions) {
        debugStep = "Build contours";
        for(auto& r : regions) {
            buildContour(field, r);
        }
    }

    void NavMeshBuilder::buildContour(const OpenHeightField& field, Region& region) {
        debugStep = Carrot::sprintf("Build contour %llu", region.index);

        // go in a direction until we hit the region's border
        int direction = Right;
        glm::ivec3 currentPosition = region.center;

        const HeightFieldSpan* pCurrentSpan = nullptr;
        do {
            const std::size_t columnIndex = currentPosition.x + currentPosition.y * sizeX;
            pCurrentSpan  = &field.at(columnIndex).spans.at(currentPosition.z);
            if(!pCurrentSpan->connected[direction]) {
                // found border of map
                break;
            }

            verify(currentPosition.x + Dx[direction] < sizeX, "There should not be a connection if we arrive at the map border");
            verify(currentPosition.y + Dy[direction] < sizeY, "There should not be a connection if we arrive at the map border");

            const std::size_t nextColumnIndex = currentPosition.x + Dx[direction] + (currentPosition.y + Dy[direction]) * sizeX;
            const auto& nextColumn = field.at(nextColumnIndex);

            bool foundNext = false;
            for(std::size_t spanIndex = 0; spanIndex < nextColumn.spans.size(); spanIndex++) {
                if(doSpansConnect(*pCurrentSpan, nextColumn.spans[spanIndex])) {
                    if(pCurrentSpan->regionID == nextColumn.spans[spanIndex].regionID) {
                        currentPosition = glm::ivec3 { currentPosition.x + Dx[direction], currentPosition.y + Dy[direction], spanIndex };
                        foundNext = true;
                        break;
                    }
                }
            }

            // found border of region
            if(!foundNext) {
                break;
            }
        } while(true);

        // at this point "currentPosition" has a position on the region border
        const glm::ivec3 startPosition = currentPosition;

        region.contour.push_back(startPosition);
        const std::size_t maxIterationCount = 200;
        std::size_t iterationCount = 0;

        std::unordered_set<glm::ivec3> alreadyVisited;
        int attemptsToAdvance = 0;
        // "hug" a wall and continue until you reach the starting position, like when trying to get to the exit of a maze
        for(; iterationCount < maxIterationCount; iterationCount++) {
            std::int64_t nextX = currentPosition.x + Dx[direction];
            std::int64_t nextY = currentPosition.y + Dy[direction];

            // out-of-bounds
            const bool outOfBounds = nextX < 0 || nextY < 0 || nextX >= sizeX || nextY >= sizeY;
            bool canAdvanceInDirection = false; // can we continue in 'direction' without leaving the region?
            bool isNextPositionConnectedOnAllSides = false;
            const HeightFieldSpan* pSpanToAdvanceTo = nullptr;
            glm::ivec3 positionToAdvanceTo;
            if(!outOfBounds) {
                std::size_t nextColumnIndex = nextX + nextY * sizeX;
                if(field.contains(nextColumnIndex)) {
                    const auto& nextColumn = field.at(nextColumnIndex);
                    for(std::size_t i = 0; i < nextColumn.spans.size(); i++) {
                        const auto& nextSpan = nextColumn.spans[i];
                        if(doSpansConnect(*pCurrentSpan, nextSpan)) {
                            if(pCurrentSpan->regionID == nextSpan.regionID) {
                                canAdvanceInDirection = true;
                                positionToAdvanceTo = { nextX, nextY, i };

                                isNextPositionConnectedOnAllSides = true;
                                for(int dir = 0; dir < DirectionCount; dir++) {
                                    isNextPositionConnectedOnAllSides &= nextSpan.connected[dir];
                                }
                                pSpanToAdvanceTo = &nextSpan;
                                break;
                            }
                        }
                    }
                }
            }

            if(canAdvanceInDirection) {
                attemptsToAdvance = 0;
                region.contour.push_back(positionToAdvanceTo);
                auto [it, isNextPositionNew] = alreadyVisited.insert(positionToAdvanceTo);
                currentPosition = positionToAdvanceTo;

                // completed the loop around the contour?
                if(startPosition == currentPosition) {
                    break;
                }

                pCurrentSpan = pSpanToAdvanceTo;

                if(isNextPositionConnectedOnAllSides || !isNextPositionNew) {
                    // going inside region, turn counter clockwise to continue against wall
                    direction = NextDirection[direction];
                }
            } else {
                // cannot go further, turn clockwise
                direction = PreviousDirection[direction];
                if(attemptsToAdvance >= DirectionCount) { // could not advance, probably a region with a single span
                    break;
                }
                attemptsToAdvance++;
            }
        }

        if(iterationCount == maxIterationCount) {
            Carrot::Log::error("Region %llu had to stop because of too many iterations!", region.index);
        }
    }

} // Carrot::AI