//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "engine/Engine.h"
#include "AccelerationStructure.h"
#include <glm/matrix.hpp>
#include <core/utils/WeakPool.hpp>
#include <core/async/Locks.h>
#include "SceneElement.h"

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

    class ASBuilder;


    enum class BLASGeometryFormat: std::uint8_t {
        Default = 0, // Carrot::Vertex
        ClusterCompressed = 1, // Carrot::PackedVertex
    };

    class BLASHandle: public WeakPoolHandle {
    public:
        bool dynamicGeometry = false;

        BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor,
                   const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                   const std::vector<glm::mat4>& transforms,
                   const std::vector<std::uint32_t>& materialSlots,
                   BLASGeometryFormat geometryFormat,
                   ASBuilder* builder);

        bool isBuilt() const { return built; }
        void update();
        void setDirty();

        virtual ~BLASHandle() noexcept override;

    private:
        std::vector<vk::AccelerationStructureGeometryKHR> geometries{};
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges{};

        std::unique_ptr<AccelerationStructure> as = nullptr;
        Carrot::BufferAllocation transformData;
        std::vector<std::shared_ptr<Carrot::Mesh>> meshes;
        std::vector<std::uint32_t> materialSlots;
        std::uint32_t firstGeometryIndex = (std::uint32_t)-1;
        bool built = false;
        ASBuilder* builder = nullptr;
        BLASGeometryFormat geometryFormat = BLASGeometryFormat::Default;

        friend class ASBuilder;
        friend class InstanceHandle;
    };

    class InstanceHandle: public WeakPoolHandle {
    public:
        InstanceHandle(std::uint32_t index,
                       std::function<void(WeakPoolHandle*)> destructor,
                       std::weak_ptr<BLASHandle> geometry,
                       ASBuilder* builder);

        bool isBuilt() const { return built; }
        bool hasBeenModified() const { return modified; }
        bool isUsable() { return enabled && !geometry.expired(); }
        void update();

        virtual ~InstanceHandle() noexcept override;

    public:
        glm::mat4 transform{1.0f};
        glm::vec4 instanceColor{1.0f};
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
        ASBuilder* builder = nullptr;

        friend class ASBuilder;
    };

    /// Helpers to build Acceleration Structures for raytracing
    // TODO: rename to RaytracingScene
    class ASBuilder: public SwapchainAware {
    public:
        explicit ASBuilder(VulkanRenderer& renderer);

        std::shared_ptr<InstanceHandle> addInstance(std::weak_ptr<BLASHandle> correspondingGeometry);

        std::shared_ptr<BLASHandle> addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                                                   const std::vector<glm::mat4>& transforms,
                                                   const std::vector<std::uint32_t>& materialSlots,
                                                   BLASGeometryFormat geometryFormat);

        void startFrame();
        void waitForCompletion(vk::CommandBuffer& cmds);

        AccelerationStructure* getTopLevelAS(const Carrot::Render::Context& renderContext);

        void onFrame(const Carrot::Render::Context& renderContext);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    public:
        /**
         * Buffer containing of vector of SceneDescription::Geometry
         */
        Carrot::BufferView getGeometriesBuffer(const Render::Context& renderContext);

        /**
         * Buffer containing of vector of SceneDescription::Instance
         */
        Carrot::BufferView getInstancesBuffer(const Render::Context& renderContext);

    private:
        void createGraveyard();
        void createSemaphores();
        void createBuildCommandBuffers();
        void createQueryPools();
        void createDescriptors();

    private:
        void buildTopLevelAS(const Carrot::Render::Context& renderContext, bool update);
        void buildBottomLevels(const Carrot::Render::Context& renderContext, const std::vector<std::shared_ptr<BLASHandle>>& toBuild);

        /// Returns the buffer view which contains an identity matrix, intended for reusing the same memory location for BLASes which do not have a specific transform
        BufferView getIdentityMatrixBufferView() const;

    private:
        VulkanRenderer& renderer;
        Async::SpinLock access;
        bool enabled = false;

    private: // reuse between builds
        std::vector<vk::UniqueCommandBuffer> tlasBuildCommands{};
        std::vector<vk::UniqueQueryPool> queryPools{};
        std::vector<std::vector<std::unique_ptr<Carrot::AccelerationStructure>>> asGraveyard; // used to store BLAS that get immediatly compacted, but need to stay alive for a few frames
        std::vector<std::vector<TracyVkCtx>> blasBuildTracyCtx; // [swapchainIndex][blasIndex]
        std::vector<std::vector<vk::UniqueCommandBuffer>> blasBuildCommands{}; // [swapchainIndex][blasIndex]
        std::vector<std::vector<vk::UniqueCommandBuffer>> compactBLASCommands{}; // [swapchainIndex][blasIndex]
        Render::PerFrame<std::unique_ptr<Carrot::Buffer>> rtInstancesBuffers;

        std::unique_ptr<Carrot::Buffer> geometriesBuffer;
        Carrot::Render::PerFrame<std::unique_ptr<Carrot::Buffer>> instancesBuffers;
        Carrot::Render::PerFrame<Carrot::BufferView> geometriesBufferPerFrame;
        Carrot::Render::PerFrame<Carrot::BufferView> instancesBufferPerFrame;

        std::size_t lastInstanceCount = 0;
        vk::DeviceAddress instanceBufferAddress = 0;

        std::unique_ptr<Carrot::Buffer> scratchBuffer = nullptr;
        std::size_t lastScratchSize = 0;
        vk::DeviceAddress scratchBufferAddress = 0;

        WeakPool<BLASHandle> staticGeometries;
        WeakPool<InstanceHandle> instances;

        Render::PerFrame<std::shared_ptr<AccelerationStructure>> tlasPerFrame;
        std::shared_ptr<AccelerationStructure> currentTLAS;

        bool builtBLASThisFrame = false;
        std::vector<vk::UniqueSemaphore> instanceUploadSemaphore;
        std::vector<vk::UniqueSemaphore> geometryUploadSemaphore;
        std::vector<vk::UniqueSemaphore> tlasBuildSemaphore;
        std::vector<vk::UniqueSemaphore> preCompactBLASSemaphore;
        std::vector<vk::UniqueSemaphore> blasBuildSemaphore;

        std::int8_t framesBeforeRebuildingTLAS = 0;
        std::size_t previousActiveInstances = 0;
        std::atomic_bool dirtyBlases = false;
        std::atomic_bool dirtyInstances = false;

        std::vector<SceneDescription::Geometry> allGeometries;

        std::size_t lastFrameIndexForTLAS = 0;
        Carrot::BufferAllocation identityMatrixForBLASes;

    private:
        std::vector<vk::BufferMemoryBarrier2KHR> bottomLevelBarriers;
        std::vector<vk::BufferMemoryBarrier2KHR> topLevelBarriers;

        friend class BLASHandle;
        friend class InstanceHandle;
    };
}
