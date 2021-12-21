//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "engine/Engine.h"
#include "AccelerationStructure.h"
#include <glm/matrix.hpp>
#include <core/utils/WeakPool.hpp>

namespace Carrot {
    struct GeometryInput {
        std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges{};

        std::unique_ptr<AccelerationStructure> as{};

        // cached structures for rebuilding
        std::unique_ptr<Buffer> scratchBuffer{};
        std::unique_ptr<vk::AccelerationStructureBuildGeometryInfoKHR> cachedBuildInfo{};
        std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> cachedBuildRanges{};
    };

    struct InstanceInput {
        glm::mat4 transform{1.0f};
        std::uint32_t customInstanceIndex;
        std::uint32_t geometryIndex;

        std::uint32_t mask;
        std::uint32_t hitGroup;
    };

    class BLASHandle: public WeakPoolHandle {
    public:
        BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, const Carrot::Mesh& mesh);

        bool isBuilt() const { return built; }
        void update();

    private:
        std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges{};

        std::unique_ptr<AccelerationStructure> as{};
        const Carrot::Mesh& mesh;
        bool built = false;

        friend class ASBuilder;
        friend class InstanceHandle;
    };

    class InstanceHandle: public WeakPoolHandle {
    public:
        InstanceHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, std::weak_ptr<BLASHandle> geometry): WeakPoolHandle(index, std::move(destructor)), geometry(geometry) {}

        bool isBuilt() const { return built; }
        bool hasBeenModified() const { return modified; }
        void update();

    public:
        glm::mat4 transform{1.0f};
        vk::GeometryInstanceFlagsKHR flags = vk::GeometryInstanceFlagBitsKHR::eForceOpaque | vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable | vk::GeometryInstanceFlagBitsKHR::eTriangleCullDisable;
        std::uint8_t mask = 0xFF;
        std::uint32_t customIndex = 0;

    private:
        glm::mat4 oldTransform{1.0f};
        vk::AccelerationStructureInstanceKHR instance;
        std::weak_ptr<BLASHandle> geometry;

        bool modified = false;
        bool built = false;

        friend class ASBuilder;
    };

    /// Helpers build Acceleration Structures for raytracing
    // TODO: rename to RaytracingScene
    class ASBuilder {
    public:
        explicit ASBuilder(VulkanRenderer& renderer);

        // TODO: mesh registration

        std::shared_ptr<InstanceHandle> addInstance(std::weak_ptr<BLASHandle> correspondingGeometry);

        std::shared_ptr<BLASHandle> addStaticMesh(const Carrot::Mesh& mesh);
        void buildTopLevelAS(bool update, bool waitForCompletion = false);

        void updateTopLevelAS();

        void startFrame();
        void waitForCompletion(vk::CommandBuffer& cmds);

        // TODO TLAS& getTopLevelAS();

        void onFrame(const Carrot::Render::Context& renderContext);

    private:
        void buildBottomLevels(const std::vector<std::shared_ptr<BLASHandle>>& toBuild, bool dynamicGeometry);

    private:
        VulkanRenderer& renderer;
        bool enabled = false;

    private: // reuse between builds
        vk::CommandBuffer tlasBuildCommands{};
        std::unique_ptr<Carrot::Buffer> instancesBuffer = nullptr;
        std::size_t lastInstanceCount = 0;
        vk::DeviceAddress instanceBufferAddress = 0;

        std::unique_ptr<Carrot::Buffer> scratchBuffer = nullptr;
        std::size_t lastScratchSize = 0;
        vk::DeviceAddress scratchBufferAddress = 0;

        WeakPool<BLASHandle> staticGeometries;
        WeakPool<InstanceHandle> instances;

        std::unique_ptr<AccelerationStructure> tlas;

    private:
        std::vector<vk::BufferMemoryBarrier2KHR> bottomLevelBarriers;
        std::vector<vk::BufferMemoryBarrier2KHR> topLevelBarriers;
    };
}
