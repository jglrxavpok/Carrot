//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "engine/Engine.h"
#include "AccelerationStructure.h"
#include <glm/matrix.hpp>
#include <core/utils/WeakPool.hpp>
#include <core/async/Locks.h>

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
        BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes, const std::vector<glm::mat4>& transforms);

        bool isBuilt() const { return built; }
        void update();
        void setDirty();

    private:
        std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges{};

        std::unique_ptr<AccelerationStructure> as = nullptr;
        std::unique_ptr<Carrot::Buffer> transformData;
        std::vector<std::shared_ptr<Carrot::Mesh>> meshes;
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
        bool enabled = true;

    private:
        glm::mat4 oldTransform{1.0f};
        vk::AccelerationStructureInstanceKHR instance;
        std::weak_ptr<BLASHandle> geometry;

        bool modified = false;
        bool built = false;

        friend class ASBuilder;
    };

    /// Helpers to build Acceleration Structures for raytracing
    // TODO: rename to RaytracingScene
    class ASBuilder: public SwapchainAware {
    public:
        explicit ASBuilder(VulkanRenderer& renderer);

        std::shared_ptr<InstanceHandle> addInstance(std::weak_ptr<BLASHandle> correspondingGeometry);

        std::shared_ptr<BLASHandle> addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes, const std::vector<glm::mat4>& transforms);

        void startFrame();
        void waitForCompletion(vk::CommandBuffer& cmds);

        std::unique_ptr<AccelerationStructure>& getTopLevelAS();

        void onFrame(const Carrot::Render::Context& renderContext);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        void createGraveyard();
        void createSemaphores();
        void createBuildCommandBuffers();
        void createQueryPools();

    private:
        void buildTopLevelAS(const Carrot::Render::Context& renderContext, bool update);
        void buildBottomLevels(const Carrot::Render::Context& renderContext, const std::vector<std::shared_ptr<BLASHandle>>& toBuild, bool dynamicGeometry);

    private:
        VulkanRenderer& renderer;
        Async::SpinLock access;
        bool enabled = false;

    private: // reuse between builds
        std::vector<vk::UniqueCommandBuffer> tlasBuildCommands{};
        std::vector<vk::UniqueQueryPool> queryPools{};
        std::vector<std::vector<std::unique_ptr<Carrot::AccelerationStructure>>> asGraveyard; // used to store BLAS that get immediatly compacted, but need to stay alive for a few frames
        std::vector<std::vector<vk::UniqueCommandBuffer>> blasBuildCommands{}; // [swapchainIndex][blasIndex]
        std::vector<std::vector<vk::UniqueCommandBuffer>> compactBLASCommands{}; // [swapchainIndex][blasIndex]
        std::unique_ptr<Carrot::Buffer> instancesBuffer = nullptr;
        std::size_t lastInstanceCount = 0;
        vk::DeviceAddress instanceBufferAddress = 0;

        std::unique_ptr<Carrot::Buffer> scratchBuffer = nullptr;
        std::size_t lastScratchSize = 0;
        vk::DeviceAddress scratchBufferAddress = 0;

        WeakPool<BLASHandle> staticGeometries;
        WeakPool<InstanceHandle> instances;

        std::unique_ptr<AccelerationStructure> tlas;

        bool builtBLASThisFrame = false;
        std::vector<vk::UniqueSemaphore> tlasBuildSemaphore;
        std::vector<vk::UniqueSemaphore> preCompactBLASSemaphore;
        std::vector<vk::UniqueSemaphore> blasBuildSemaphore;

        std::int8_t framesBeforeRebuildingTLAS = 0;
        std::size_t previousActiveInstances = 0;

    private:
        std::vector<vk::BufferMemoryBarrier2KHR> bottomLevelBarriers;
        std::vector<vk::BufferMemoryBarrier2KHR> topLevelBarriers;
    };
}
