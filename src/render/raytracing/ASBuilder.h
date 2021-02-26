//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "Engine.h"
#include "render/raytracing/AccelerationStructure.h"
#include <glm/matrix.hpp>

namespace Carrot {
    struct GeometryInput {
        vector<vk::AccelerationStructureGeometryKHR> geometries{};
        vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges{};

        unique_ptr<AccelerationStructure> as{};

        // cached structures for rebuilding
        unique_ptr<Buffer> scratchBuffer{};
        unique_ptr<vk::AccelerationStructureBuildGeometryInfoKHR> cachedBuildInfo{};
        vector<const vk::AccelerationStructureBuildRangeInfoKHR*> cachedBuildRanges{};
    };

    struct InstanceInput {
        glm::mat4 transform{1.0f};
        uint32_t customInstanceIndex;
        uint32_t geometryIndex;

        uint32_t mask;
        uint32_t hitGroup;
    };

    struct TLAS {
        unique_ptr<AccelerationStructure> as{};
    };

    /// Helpers build Acceleration Structures for raytracing
    class ASBuilder {
    private:
        Engine& engine;
        vector<GeometryInput> bottomLevelGeometries{};
        vector<InstanceInput> topLevelInstances{};
        unique_ptr<AccelerationStructure> topLevelAS{};
        unique_ptr<Buffer> instancesBuffer{};
        TLAS tlas{};

        vk::AccelerationStructureInstanceKHR convertToVulkanInstance(const InstanceInput& instance);

    public:
        explicit ASBuilder(Engine& engine);

        template<typename VertexType>
        vector<Carrot::GeometryInput*> addModelGeometries(const Model& model);

        ///
        /// \param indexBuffer
        /// \param indexOffset
        /// \param vertexBuffer
        /// \param vertexOffsets offset of vertices inside the vertex buffer, in bytes
        template<typename VertexType>
        Carrot::GeometryInput* addGeometries(const Buffer& indexBuffer, uint64_t indexCount, uint64_t indexOffset, const Buffer& vertexBuffer, uint64_t vertexCount, const vector<uint64_t>& vertexOffsets);

        void addInstance(const InstanceInput input);

        void buildBottomLevelAS(bool enableUpdate = true);
        void buildTopLevelAS(bool update);

        void updateBottomLevelAS(const vector<size_t>& blasIndices);
        void updateTopLevelAS();

        TLAS& getTopLevelAS();

        const vector<GeometryInput>& getBottomLevelGeometries() const;
        vector<GeometryInput>& getBottomLevelGeometries();
    };
}

#include "ASBuilder.ipp"