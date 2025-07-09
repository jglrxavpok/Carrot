//
// Created by jglrxavpok on 30/12/2020.
//

#include "ASBuilder.h"
#include <engine/render/resources/Mesh.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>
#include <core/allocators/InlineAllocator.h>
#include <core/allocators/FallbackAllocator.h>
#include <core/allocators/MallocAllocator.h>
#include <core/allocators/StackAllocator.h>
#include <core/io/Logging.hpp>
#include <core/math/BasicFunctions.h>

#include "RayTracer.h"
#include "engine/Capabilities.h"
#include "engine/Engine.h"
#include "engine/render/resources/ResourceAllocator.h"

namespace Carrot {
    static constexpr glm::mat4 IdentityMatrix = glm::identity<glm::mat4>();

    BLASHandle::BLASHandle(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                           const std::vector<glm::mat4>& transforms,
                           const std::vector<std::uint32_t>& materialSlots,
                           BLASGeometryFormat geometryFormat,
                           const Render::PrecomputedBLAS* pPrecomputedBLAS,
                           ASBuilder* builder):
    meshes(meshes), materialSlots(materialSlots), builder(builder), geometryFormat(geometryFormat), pPrecomputedBLAS(pPrecomputedBLAS) {
        bool allIdentity = true;
        for(const auto& transform : transforms) {
            for (int x = 0; x < 4; ++x) {
                for (int y = 0; y < 4; ++y) {
                    if(glm::abs(transform[x][y] - IdentityMatrix[x][y]) > 10e-6) {
                        allIdentity = false;
                        break;
                    }
                }
                if(!allIdentity) {
                    break;
                }
            }
            if(!allIdentity) {
                break;
            }
        }

        Carrot::Vector<vk::DeviceAddress> transformAddresses;
        transformAddresses.resize(meshes.size());
        if(!allIdentity) {
            transformData = GetResourceAllocator().allocateDeviceBuffer(
                    transforms.size() * sizeof(vk::TransformMatrixKHR),
                    vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);

            FallbackAllocator<InlineAllocator<sizeof(vk::TransformMatrixKHR)*4>, MallocAllocator> stackAlloc; // with virtual geometry, most calls will be at most 4 meshes at once
            Carrot::Vector<vk::TransformMatrixKHR> rtTransforms { stackAlloc };
            rtTransforms.resize(transforms.size());
            vk::DeviceAddress baseTransformDataAddress = transformData.view.getDeviceAddress();

            for (int j = 0; j < transforms.size(); ++j) {
                rtTransforms[j] = ASBuilder::glmToRTTransformMatrix(transforms[j]);

                transformAddresses[j] = baseTransformDataAddress + j * sizeof(vk::TransformMatrixKHR);
            }

            // TODO: GP Direct
            //transformData.view.uploadForFrame(std::span<const vk::TransformMatrixKHR>(rtTransforms));
            transformData.view.stageUpload(std::span<const vk::TransformMatrixKHR>(rtTransforms));
        } else {
            vk::DeviceAddress transformDataAddress = builder->getIdentityMatrixBufferView().getDeviceAddress();
            transformAddresses.fill(transformDataAddress);
        }

        innerInit(transformAddresses);
    }

    BLASHandle::BLASHandle(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                           const std::vector<vk::DeviceAddress>& transforms,
                           const std::vector<std::uint32_t>& materialSlots,
                           BLASGeometryFormat geometryFormat,
                           const Render::PrecomputedBLAS* pPrecomputedBLAS,
                           ASBuilder* builder):
    meshes(meshes), materialSlots(materialSlots), builder(builder), geometryFormat(geometryFormat), pPrecomputedBLAS(pPrecomputedBLAS) {
        innerInit(transforms);
    }

    void BLASHandle::innerInit(std::span<const vk::DeviceAddress/*vk::TransformMatrixKHR*/> transformAddresses) {
        geometries.reserve(meshes.size());
        buildRanges.reserve(meshes.size());

        verify(meshes.size() == transformAddresses.size(), "Need as many meshes as there are transforms!");
        verify(meshes.size() == materialSlots.size(), "Need as many material handles as there are meshes!");

        for(std::size_t i = 0; i < meshes.size(); i++) {
            auto& meshPtr = meshes[i];

            auto vertexAddress = meshPtr->getVertexBuffer().getDeviceAddress();
            auto indexAddress = meshPtr->getIndexBuffer().getDeviceAddress();
            auto primitiveCount = meshPtr->getIndexCount() / 3; // all triangles

            auto& geometry = geometries.emplace_back();
            geometry.geometryType = vk::GeometryTypeKHR::eTriangles;

            vk::IndexType indexType = vk::IndexType::eNoneKHR;
            if(meshPtr->getSizeOfSingleIndex() == sizeof(std::uint32_t)) {
                indexType = vk::IndexType::eUint32;
            } else if(meshPtr->getSizeOfSingleIndex() == sizeof(std::uint16_t)) {
                indexType = vk::IndexType::eUint16;
            } else {
                verify(false, Carrot::sprintf("Unsupported index size for raytracing: %llu", meshPtr->getSizeOfSingleIndex())); // unsupported
            }

            // if all identity, always point to the same location in GPU memory
            vk::DeviceAddress specificTransformAddress = transformAddresses[i];
            geometry.geometry = vk::AccelerationStructureGeometryTrianglesDataKHR {
                    // vec3 triangle position, other attributes are passed via the vertex buffer used as a storage buffer (via descriptors)
                    .vertexFormat = vk::Format::eR32G32B32Sfloat,
                    .vertexData = vertexAddress,
                    .vertexStride = meshPtr->getSizeOfSingleVertex(),
                    .maxVertex = static_cast<uint32_t>(meshPtr->getVertexCount()-1),
                    .indexType = indexType,
                    .indexData = indexAddress,
                    .transformData = specificTransformAddress,
            };

            buildRanges.emplace_back(vk::AccelerationStructureBuildRangeInfoKHR {
                    .primitiveCount = static_cast<uint32_t>(primitiveCount),
                    .primitiveOffset = 0,
                    .firstVertex = 0,
                    .transformOffset = 0,
            });
        }

        //builder->dirtyBlases = true;
    }

    BLASHandle::~BLASHandle() noexcept {
        //builder->dirtyBlases = true;
    }

    void BLASHandle::update() {
        // no op for now
    }

    void BLASHandle::setDirty() {
        built = false;
    }

    void BLASHandle::bindSemaphores(const Render::PerFrame<vk::Semaphore>& boundSemaphores) {
        this->boundSemaphores = boundSemaphores;
    }

    vk::Semaphore BLASHandle::getBoundSemaphore(std::size_t swapchainIndex) const {
        if(boundSemaphores.empty()) {
            return {};
        }
        return boundSemaphores[swapchainIndex % boundSemaphores.size()];
    }

    InstanceHandle::InstanceHandle(std::shared_ptr<BLASHandle> geometry, ASBuilder* builder):
            geometry(geometry), builder(builder) {
        builder->dirtyInstances = true;
    }

    InstanceHandle::~InstanceHandle() noexcept {
        builder->dirtyInstances = true;
    }

    void InstanceHandle::update() {
        #define setAndCheck(out, newValue) \
            do { if((out) != (newValue)) { \
                modified = true; \
                (out) = (newValue); \
            } } while(0)

        auto transposedTransform = glm::transpose(transform);
        // extract 4x3 matrix
        std::memcpy(&instance.transform, glm::value_ptr(transposedTransform), sizeof(instance.transform));

        setAndCheck(oldTransform, transform);
        setAndCheck(instance.instanceCustomIndex, customIndex);

        setAndCheck(instance.flags, static_cast<VkGeometryInstanceFlagsKHR>(flags));

        setAndCheck(instance.instanceShaderBindingTableRecordOffset, 0); // TODO do we need to reintroduce hit groups?
        setAndCheck(instance.mask, mask);

        auto blasAddress = geometry->as->getDeviceAddress();
        setAndCheck(instance.accelerationStructureReference, blasAddress);
    }
}

// --

Carrot::ASBuilder::PerThreadCommandObjects::~PerThreadCommandObjects() {
    TracyVkDestroy(tracyCtx);
}

Carrot::ASBuilder::ASBuilder(Carrot::VulkanRenderer& renderer): renderer(renderer) {
    enabled = GetCapabilities().supportsRaytracing;
    vk::PhysicalDeviceAccelerationStructurePropertiesKHR properties = renderer.getVulkanDriver().getPhysicalDevice().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceAccelerationStructurePropertiesKHR>().get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>();
    scratchBufferAlignment = properties.minAccelerationStructureScratchOffsetAlignment;

    onSwapchainImageCountChange(GetEngine().getSwapchainImageCount());

    identityMatrixForBLASes = GetResourceAllocator().allocateDeviceBuffer(sizeof(vk::TransformMatrixKHR), vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR);
    vk::TransformMatrixKHR identityValue;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 3; ++row) {
            identityValue.matrix[row][column] = IdentityMatrix[column][row];
        }
    }
    identityMatrixForBLASes.view.stageUpload(std::span<const vk::TransformMatrixKHR>{ &identityValue, 1 });
}

Carrot::ASBuilder::~ASBuilder() {
    for(auto& ctx : tlasBuildTracyCtx) {
        TracyVkDestroy(ctx);
    }
}


void Carrot::ASBuilder::createBuildCommandBuffers() {
    tlasBuildCommands = GetVulkanDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
            .commandPool = renderer.getVulkanDriver().getThreadComputeCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = GetEngine().getSwapchainImageCount(),
    });

    tlasBuildTracyCtx.resize(GetEngine().getSwapchainImageCount());
    for(std::size_t i = 0; i < tlasBuildTracyCtx.size(); i++) {
        tlasBuildTracyCtx[i] = GetEngine().createTracyContext(Carrot::sprintf("TLAS build %llu", i));
    }

    blasBuildTracyContextes.resize(GetEngine().getSwapchainImageCount());
    for(std::size_t imageIndex = 0; imageIndex < GetEngine().getSwapchainImageCount(); imageIndex++) {
        for(std::size_t i = 0; i < BLASBuildBucketCount; i++) {
            blasBuildTracyContextes[imageIndex][i] = GetEngine().createTracyContext(Carrot::sprintf("BLAS bucket %d", i));
        }
    }
}

void Carrot::ASBuilder::createQueryPools() {
    queryPools.resize(GetEngine().getSwapchainImageCount());
}

void Carrot::ASBuilder::createGraveyard() {
}

void Carrot::ASBuilder::createDescriptors() {
    geometriesBuffer = nullptr;
    instancesBufferPerFrame.clear();
}

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                                                                      const std::vector<vk::DeviceAddress>& transformAddresses,
                                                                      const std::vector<std::uint32_t>& materials,
                                                                      BLASGeometryFormat geometryFormat,
                                                                      const Render::PrecomputedBLAS* pPrecomputedBLAS) {
    if(!enabled)
        return nullptr;

    ASStorage<BLASHandle>::Reservation slot = staticGeometries.reserveSlot();
    std::shared_ptr<BLASHandle> ptr = std::make_shared<BLASHandle>(meshes, transformAddresses, materials, geometryFormat, pPrecomputedBLAS, this);
    *slot = ptr;
    return ptr;
}

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                                                                      const std::vector<glm::mat4>& transforms,
                                                                      const std::vector<std::uint32_t>& materials,
                                                                      BLASGeometryFormat geometryFormat,
                                                                      const Render::PrecomputedBLAS* precomputedBLAS) {
    if(!enabled)
        return nullptr;
    ASStorage<BLASHandle>::Reservation slot = staticGeometries.reserveSlot();
    auto ptr = std::make_shared<BLASHandle>(meshes, transforms, materials, geometryFormat, precomputedBLAS, this);
    *slot = ptr;
    return ptr;
}

std::shared_ptr<Carrot::InstanceHandle> Carrot::ASBuilder::addInstance(std::shared_ptr<Carrot::BLASHandle> correspondingGeometry) {
    if(!enabled)
        return nullptr;
    ASStorage<InstanceHandle>::Reservation slot = instances.reserveSlot();
    auto ptr = std::make_shared<InstanceHandle>(correspondingGeometry, this);;
    *slot = ptr;
    return ptr;
}

void Carrot::ASBuilder::createSemaphores() {
    std::size_t imageCount = GetEngine().getSwapchainImageCount();
    preCompactBLASSemaphore.resize(imageCount);
    blasBuildSemaphore.resize(imageCount);
    tlasBuildSemaphore.resize(imageCount);
    instanceUploadSemaphore.resize(imageCount);
    serializedCopySemaphore.resize(imageCount);

    for (size_t i = 0; i < imageCount; ++i) {
        preCompactBLASSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        blasBuildSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        tlasBuildSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        instanceUploadSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        serializedCopySemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});

        DebugNameable::nameSingle("Precompact BLAS build", *preCompactBLASSemaphore[i]);
        DebugNameable::nameSingle(Carrot::sprintf("BLAS build %d", (int)i), *blasBuildSemaphore[i]);
        DebugNameable::nameSingle("TLAS build", *tlasBuildSemaphore[i]);
        DebugNameable::nameSingle("Instance upload", *instanceUploadSemaphore[i]);
        DebugNameable::nameSingle("SerializedAS copy", *serializedCopySemaphore[i]);
    }

    vk::SemaphoreTypeCreateInfo timelineCreateInfo {
        .semaphoreType = vk::SemaphoreType::eTimeline,
        .initialValue = 0,
    };
    tlasBuildTimelineSemaphore = GetVulkanDevice().createSemaphoreUnique(vk::SemaphoreCreateInfo {
        .pNext = &timelineCreateInfo
    });
    DebugNameable::nameSingle("TLAS build", *tlasBuildTimelineSemaphore);
}

void Carrot::ASBuilder::onFrame(const Carrot::Render::Context& renderContext) {
    if(!enabled)
        return;
    if(renderContext.pViewport != &GetEngine().getMainViewport()) {
        return;
    }
    ScopedMarker("ASBuilder::onFrame");

    if(!preallocatedBuildCommandBuffers) {
        for(const auto& threadID : GetTaskScheduler().getParallelThreadIDs()) {
            preallocateBlasBuildCommandBuffers(threadID);
        }
        preallocateBlasBuildCommandBuffers(std::this_thread::get_id()); // main thread
        preallocatedBuildCommandBuffers = true;
    }
    std::vector<std::shared_ptr<BLASHandle>> toBuild;
    staticGeometries.iterate([&](std::shared_ptr<BLASHandle> pValue) {
        if(toBuild.size() > 1000) {
            return;
        }
        if(!pValue->isBuilt() || dirtyBlases) {
            toBuild.push_back(pValue->shared_from_this());
            pValue->built = false;
        }
    });

    builtBLASThisFrame = false;
    if(dirtyBlases) {
        allGeometries.clear();
    }
    if(!toBuild.empty()) {
        buildBottomLevels(renderContext, toBuild);
        for(auto& v : toBuild) {
            v->built = true;
        }
    } else {
        // ensure we still get Tracy's profiling data even when not building BLASes
        GetEngine().getVulkanDriver().performSingleTimeComputeCommands([&](vk::CommandBuffer& commands) {
            for(auto& ctx : blasBuildTracyContextes[renderContext.swapchainIndex]) {
                TracyVkCollect(ctx, commands);
            }
        }, false /* don't wait, just go */);
    }
    dirtyBlases = false;

    std::size_t activeInstances = 0;

    instances.iterate([&](std::shared_ptr<InstanceHandle> value) {
        if(value->isUsable()) {
            value->update();
            activeInstances++;
        }
    });

    bool requireTLASRebuildNow = !toBuild.empty() || dirtyInstances || activeInstances != previousActiveInstances;
    previousActiveInstances = activeInstances;

    dirtyInstances = false;
    if(requireTLASRebuildNow) {
        buildTopLevelAS(renderContext, false);
        framesBeforeRebuildingTLAS = 11;
    } else {
        // TODO merge with requireTLASRebuildNow
        if(framesBeforeRebuildingTLAS == 0 || !currentTLAS) {
            buildTopLevelAS(renderContext, false);
            framesBeforeRebuildingTLAS = 20;
        } else {
            if(framesBeforeRebuildingTLAS > 0) {
                framesBeforeRebuildingTLAS--;
            }
            buildTopLevelAS(renderContext, true);
        }
    }

    if(geometriesBuffer) {
        geometriesBufferPerFrame[renderContext.swapchainIndex] = geometriesBuffer;
    }

    if(instancesBuffer) {
        instancesBufferPerFrame[renderContext.swapchainIndex] = instancesBuffer;
    }

    if(rtInstancesBuffer) {
        rtInstancesBufferPerFrame[renderContext.swapchainIndex] = rtInstancesBuffer;
    }

    if(rtInstancesScratchBuffer) {
        rtInstancesScratchBufferPerFrame[renderContext.swapchainIndex] = rtInstancesScratchBuffer;
    }

    tlasBuildTimelineSemaphoreWaitValuesForRenderThread[renderContext.swapchainIndex] = tlasBuildTimelineSemaphoreWaitValue;
    tlasPerFrame[renderContext.swapchainIndex] = currentTLAS;
}

void Carrot::ASBuilder::resetBlasBuildCommands(const Carrot::Render::Context& renderContext) {
    auto& perThreadCommandPools = blasBuildCommandObjects[renderContext.swapchainIndex];
    if(!perThreadCommandPools) {
        perThreadCommandPools = std::make_unique<Carrot::Async::ParallelMap<std::thread::id, PerThreadCommandObjects>>();
    }
    for(auto& [_, ppPool] : perThreadCommandPools->snapshot()) {
        renderer.getLogicalDevice().resetCommandPool(*ppPool->commandPool);
    }
}

void Carrot::ASBuilder::preallocateBlasBuildCommandBuffers(const std::thread::id& threadID) {
    for(std::size_t imageIndex = 0; imageIndex < GetEngine().getSwapchainImageCount(); imageIndex++) {
        auto& perThreadCommandPools = blasBuildCommandObjects[imageIndex];
        if(!perThreadCommandPools) {
            perThreadCommandPools = std::make_unique<Carrot::Async::ParallelMap<std::thread::id, PerThreadCommandObjects>>();
        }
        perThreadCommandPools->getOrCompute(threadID, [&]() {
            PerThreadCommandObjects result;
            result.commandPool = renderer.getVulkanDriver().createComputeCommandPool();
            result.tracyCtx = GetEngine().createTracyContext("BLAS build"); // TODO: pool
            return result;
        });
    }
}

vk::CommandBuffer Carrot::ASBuilder::getBlasBuildCommandBuffer(const Carrot::Render::Context& renderContext) {
    auto& perThreadCommandPools = blasBuildCommandObjects[renderContext.swapchainIndex];
    std::thread::id currentThreadID = std::this_thread::get_id();
    PerThreadCommandObjects& objects = perThreadCommandPools->getOrCompute(currentThreadID, [&]() {
        PerThreadCommandObjects result;
        result.commandPool = renderer.getVulkanDriver().createComputeCommandPool();
        result.tracyCtx = GetEngine().createTracyContext("BLAS build");
        return result;
    });
    auto result = renderer.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
        .commandPool = *objects.commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
    });
    return result[0];
}

void Carrot::ASBuilder::buildBottomLevels(const Carrot::Render::Context& renderContext, const std::vector<std::shared_ptr<BLASHandle>>& toBuild) {
    ScopedMarker("buildBottomLevels");
    if(!enabled)
        return;

    auto& device = renderer.getLogicalDevice();
    const std::size_t semaphoreIndex = renderContext.frameCount % tlasBuildCommands.size();

    // add a bottom level AS for each geometry entry
    std::size_t blasCount = toBuild.size();
    if(blasCount == 0) {
        return;
    }

    resetBlasBuildCommands(renderContext);

    for(auto& storagePerBucket : prebuiltBLASStorages[renderContext.swapchainIndex]) {
        storagePerBucket = {};
    }
    struct BLASDeserialization {
        vk::CopyMemoryToAccelerationStructureInfoKHR copyInfo{};
    };
    struct BLASBucket {
        Cider::Mutex access;
        vk::CommandBuffer cmds; // where to register build commands for this bucket

        Carrot::Vector<BLASDeserialization> prebuiltContents; // empty is no prebuilt AS for a given index
        Carrot::Vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfos;
        Carrot::Vector<const vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRanges;

        TracyVkCtx tracyContext;
    };

    struct PerBlas {
        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo;
        vk::DeviceSize allocationSize;
        vk::DeviceSize serializedSize;
        vk::DeviceSize storageOffset; // scratch offset if build, serialized data buffer if prebuilt
        Carrot::BufferAllocation asMemory;
        std::uint8_t bucketIndex : 6;
        bool firstBuild: 1 = false;
        bool compatiblePrecomputed: 1 = false;
    };
    static_assert(BLASBuildBucketCount < (1<<6), "too many buckets for current PerBlas structure, because of 6bit bucket index. Change structure if need more buckets");

    // TODO: reuse memory
    Carrot::Vector<BLASBucket> buckets{BLASBuildBucketCount};

    Carrot::Vector<PerBlas> perBlas;
    perBlas.resize(blasCount);

    for(std::size_t blasIndex = 0; blasIndex < blasCount; blasIndex++) {
        perBlas[blasIndex].bucketIndex = (blasIndex / BLASBuildBucketCount) % BLASBuildBucketCount;
    }

    vk::UniqueQueryPool& queryPool = queryPools[renderContext.swapchainIndex];

    std::atomic_bool hasNewGeometry{ false };

    for (std::size_t bucketIndex = 0; bucketIndex < BLASBuildBucketCount; bucketIndex++) {
        auto& bucket = buckets[bucketIndex];
        bucket.tracyContext = blasBuildTracyContextes[renderContext.swapchainIndex][bucketIndex];
        bucket.buildInfos.setGrowthFactor(2);
        bucket.pBuildRanges.setGrowthFactor(2);
        bucket.prebuiltContents.setGrowthFactor(2);
    }
/*TODO: queries
       if(buildCommands.size() < blasCount) {
        vk::QueryPoolCreateInfo queryPoolCreateInfo{
                .queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
                .queryCount = static_cast<uint32_t>(BucketCount),
        };

        queryPool = GetVulkanDevice().createQueryPoolUnique(queryPoolCreateInfo, GetVulkanDriver().getAllocationCallbacks());
    }*/


    // TODO: fast build for virtual geometry
    auto getBuildFlags = [](BLASHandle& blas) {
        return blas.dynamicGeometry ?
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate) :
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction);
    };

    const std::size_t blasGranularity = static_cast<std::size_t>(ceil(static_cast<double>(blasCount) / BLASBuildBucketCount));
    std::atomic<std::size_t> newGeometriesCount = 0;
    GetTaskScheduler().parallelFor(blasCount, [&](std::size_t index) {
        auto& blas = *toBuild[index];
        perBlas[index].firstBuild = dirtyBlases || blas.firstGeometryIndex == (std::uint32_t) -1;
        if (perBlas[index].firstBuild) {
            newGeometriesCount += blas.geometries.size();
        }
    }, blasGranularity);

    // resize + threads + atomic increment => allGeometries is guaranteed to already have the necessary allocation
    std::atomic_uint32_t geometryIndex = allGeometries.size();
    allGeometries.resize(allGeometries.size() + newGeometriesCount);

    std::atomic<vk::DeviceSize> scratchSize = 0;
    std::atomic<vk::DeviceSize> totalSerializedSize = 0;

    GetTaskScheduler().parallelFor(blasCount, [&](std::size_t index) {
        ScopedMarker("Prepare BLASes");

        auto& info = perBlas[index].buildInfo;
        auto& blas = *toBuild[index];

        const vk::BuildAccelerationStructureFlagsKHR flags = getBuildFlags(blas);

        if (perBlas[index].firstBuild) {
            std::size_t geometryIndexStart = geometryIndex.fetch_add(blas.geometries.size());
            blas.firstGeometryIndex = geometryIndexStart;
            for (std::size_t j = 0; j < blas.geometries.size(); ++j) {
                allGeometries[j + geometryIndexStart] = (SceneDescription::Geometry {
                        .vertexBufferAddress = blas.geometries[j].geometry.triangles.vertexData.deviceAddress,
                        .indexBufferAddress = blas.geometries[j].geometry.triangles.indexData.deviceAddress,
                        .materialIndex = blas.materialSlots[j],
                        .geometryFormat = blas.geometryFormat,
                });
            }
            hasNewGeometry.store(true);

            if(blas.pPrecomputedBLAS != nullptr) {
                // attempt to see if we can reuse the precomputed BLAS
                vk::AccelerationStructureVersionInfoKHR versionInfo {
                    .pVersionData = blas.pPrecomputedBLAS->blasBytes.data(),
                };
                vk::AccelerationStructureCompatibilityKHR compatibility = vk::AccelerationStructureCompatibilityKHR::eIncompatible;
                // TODO: might be possible to offload this to model loading and/or cache results
                GetVulkanDriver().getLogicalDevice().getAccelerationStructureCompatibilityKHR(&versionInfo, &compatibility);
                if(compatibility == vk::AccelerationStructureCompatibilityKHR::eCompatible) {
                    verify(blas.pPrecomputedBLAS->getHeader().accelerationStructureSerializedSize == blas.pPrecomputedBLAS->blasBytes.size(), "Header and actual data don't have the same size?");
                    perBlas[index].allocationSize = blas.pPrecomputedBLAS->getHeader().accelerationStructureRuntimeSize;
                    perBlas[index].serializedSize = blas.pPrecomputedBLAS->getHeader().accelerationStructureSerializedSize;
                    perBlas[index].storageOffset = totalSerializedSize.fetch_add(Carrot::Math::alignUp(perBlas[index].serializedSize, scratchBufferAlignment) /* ensure all offsets are aligned */);
                    perBlas[index].compatiblePrecomputed = true;
                    return;
                }
            }
        }

        info.geometryCount = blas.geometries.size();
        info.flags = flags;
        info.pGeometries = blas.geometries.data();
        info.mode = perBlas[index].firstBuild ? vk::BuildAccelerationStructureModeKHR::eBuild : vk::BuildAccelerationStructureModeKHR::eUpdate;
        info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        info.srcAccelerationStructure = perBlas[index].firstBuild ? nullptr : blas.as->getVulkanAS();

        static thread_local Carrot::StackAllocator allocator{ Carrot::Allocator::getDefault(), 4096 };
        allocator.clear();

        Carrot::Vector<uint32_t> primitiveCounts{ allocator };
        {
            ZoneScopedN("Fill primitive counts");

            {
                ZoneScopedN("Vector allocation");
                primitiveCounts.resize(blas.buildRanges.size());
            }
            // copy primitive counts to flat vector
            for(size_t geomIndex = 0; geomIndex < primitiveCounts.size(); geomIndex++) {
                primitiveCounts[geomIndex] = blas.buildRanges[geomIndex].primitiveCount;
            }
        }

        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo;
        {
            ZoneScopedN("Compute AS size");
            sizeInfo = device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, info, primitiveCounts);
        }

        perBlas[index].allocationSize = sizeInfo.accelerationStructureSize;

        // allocate slightly more to ensure all offsets are aligned to 'scratchBufferAlignment'
        vk::DeviceSize requiredSize = Carrot::Math::alignUp(perBlas[index].firstBuild ? sizeInfo.buildScratchSize : sizeInfo.updateScratchSize, scratchBufferAlignment);
        perBlas[index].storageOffset = scratchSize.fetch_add(requiredSize);

    }, blasGranularity);

    // From what I understand from the Vulkan spec, the buffer containing the data for the serialized AS must already be on device, so we have an intermediate copy
    Carrot::BufferAllocation& serializedASStorage = prebuiltBLASStorages[renderContext.swapchainIndex][0];
    Carrot::BufferAllocation& serializedASStorageStaging = prebuiltBLASStorages[renderContext.swapchainIndex][1];
    {
        ZoneScopedN("Allocate memory for BLASes");
        GetResourceAllocator().multiAllocateDeviceBuffer(perBlas.size(), sizeof(PerBlas),
            reinterpret_cast<BufferAllocation*>(reinterpret_cast<std::byte*>(perBlas.data())+offsetof(PerBlas, asMemory)),
            reinterpret_cast<vk::DeviceSize*>(reinterpret_cast<std::byte*>(perBlas.data())+offsetof(PerBlas, allocationSize)),
            vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);

        if(totalSerializedSize > 0) {
            serializedASStorage = GetResourceAllocator().allocateDeviceBuffer(totalSerializedSize, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eTransferDst);
            serializedASStorageStaging = GetResourceAllocator().allocateStagingBuffer(totalSerializedSize);

            serializedASStorage.name("serializedASStorage");
            serializedASStorageStaging.name("serializedASStorageStaging");
        }
    }

    GetTaskScheduler().parallelFor(blasCount, [&](std::size_t index) {
        auto& info = perBlas[index].buildInfo;
        auto& blas = *toBuild[index];

        vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = perBlas[index].allocationSize,
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        };

        blas.as = std::move(std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo, std::move(perBlas[index].asMemory)));
        info.dstAccelerationStructure = blas.as->getVulkanAS();
    }, static_cast<std::size_t>(ceil(static_cast<double>(blasCount) / BLASBuildBucketCount)));

    if(hasNewGeometry.load()) {
        geometriesBuffer = std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(sizeof(SceneDescription::Geometry) * allGeometries.size(),
                                                                          vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)));
        geometriesBuffer->name("RT Geometries");

        geometriesBuffer->view.uploadForFrame(std::span<const SceneDescription::Geometry> { allGeometries });
    }

    auto& queueFamilies = GetVulkanDriver().getQueueFamilies();
    // TODO: reuse
    // TODO: VkPhysicalDeviceAccelerationStructurePropertiesKHR::minAccelerationStructureScratchOffsetAlignment
    UniquePtr<Buffer> pBlasBuildScratchBuffer = scratchSize > 0
        ? GetResourceAllocator().allocateDedicatedBuffer(scratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, std::set{queueFamilies.computeFamily.value()})
        : nullptr;
    vk::DeviceAddress scratchAddress = 0x999999999999DEAD;
    if(pBlasBuildScratchBuffer) {
        pBlasBuildScratchBuffer->name("BLAS build scratch buffer");
        scratchAddress = device.getBufferAddress({.buffer = pBlasBuildScratchBuffer->getVulkanBuffer() });
    }

    bool hasASToCompact = false;
    // TODO: only dependent on these tasks is rendering, right? + TLAS build
    // TODO: timeline semaphore might be necessary
    GetTaskScheduler().parallelFor(BLASBuildBucketCount, [&](std::size_t bucketIndex) {
    //for(std::size_t index = 0; index < blasCount; index++) {
        ZoneScopedN("Process bucket");
        ZoneValue(bucketIndex);
        BLASBucket& bucket = buckets[bucketIndex];
        {
            ZoneScopedN("Put BLASes in bucket");

            for(std::size_t blasStartIndex = bucketIndex * BLASBuildBucketCount; blasStartIndex < blasCount; blasStartIndex += BLASBuildBucketCount*BLASBuildBucketCount) {
                for(std::size_t blasIndex = blasStartIndex; blasIndex < blasStartIndex + BLASBuildBucketCount && blasIndex < blasCount; blasIndex++) {
                    if(perBlas[blasIndex].compatiblePrecomputed) {
                        ZoneScopedN("Prepare prebuilt");

                        BLASDeserialization& deserialization = bucket.prebuiltContents.emplaceBack();
                        const Render::PrecomputedBLAS& precomputedBLAS = *toBuild[blasIndex]->pPrecomputedBLAS;

                        Carrot::BufferView stagingBuffer = serializedASStorageStaging.view.subView(perBlas[blasIndex].storageOffset, perBlas[blasIndex].serializedSize);
                        Carrot::BufferView deviceBuffer = serializedASStorage.view.subView(perBlas[blasIndex].storageOffset, perBlas[blasIndex].serializedSize);

                        std::uint8_t* pStagingData = stagingBuffer.map<std::uint8_t>();
                        memcpy(pStagingData, precomputedBLAS.blasBytes.data(), precomputedBLAS.blasBytes.size());

                        deserialization.copyInfo = vk::CopyMemoryToAccelerationStructureInfoKHR {
                            .src = deviceBuffer.getDeviceAddress(),
                            .dst = toBuild[blasIndex]->as->getVulkanAS(),
                            .mode = vk::CopyAccelerationStructureModeKHR::eDeserialize,
                        };
                    } else {
                        // initiate build
                        perBlas[blasIndex].buildInfo.scratchData.deviceAddress = scratchAddress + perBlas[blasIndex].storageOffset;

                        bucket.buildInfos.emplaceBack(perBlas[blasIndex].buildInfo);
                        bucket.pBuildRanges.emplaceBack(toBuild[blasIndex]->buildRanges.data());
                    }
                }
            }

            bool compactAS = false; // TODO: compaction (getBuildFlags(*toBuild[index]) & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) == vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
            compactAS = false;
            hasASToCompact |= compactAS;

            // TODO: compaction cmds->resetQueryPool(*queryPool, index, 1);

            // write compacted size
            if(compactAS) {
                // TODO: compaction cmds->writeAccelerationStructuresPropertiesKHR(toBuild[index]->as->getVulkanAS(), vk::QueryType::eAccelerationStructureCompactedSizeKHR, *queryPool, index);
            }
        }

        {
            ZoneScopedN("Record bucket");
            if(!bucket.buildInfos.empty() || !bucket.prebuiltContents.empty()) {
                auto& cmds = bucket.cmds;
                TracyVkCtx tracyCtx = bucket.tracyContext;
                cmds = getBlasBuildCommandBuffer(renderContext);
                cmds.begin(vk::CommandBufferBeginInfo {
                    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                    .pInheritanceInfo = nullptr,
                });
                {
                    TracyVkZone(tracyCtx, cmds, "Build BLAS");

                    GetVulkanDriver().setMarker(cmds, "Begin AS build");

                    if(!bucket.buildInfos.empty()) {
                        ZoneScopedN("Record AS build");
                        TracyVkZone(tracyCtx, cmds, "AS build");
                        cmds.buildAccelerationStructuresKHR(bucket.buildInfos, bucket.pBuildRanges);
                    }
                    if(!bucket.prebuiltContents.empty()) {
                        ZoneScopedN("Record AS copies");
                        TracyVkZone(tracyCtx, cmds, "AS copies");

                        vk::BufferMemoryBarrier barrier{};
                        barrier.buffer = serializedASStorage.view.getVulkanBuffer();
                        barrier.offset = serializedASStorage.view.getStart();
                        barrier.size = serializedASStorage.view.getSize();
                        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
                        barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;

                        cmds.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, static_cast<vk::DependencyFlags>(0), {}, barrier, {});

                        for(const BLASDeserialization& deserialization : bucket.prebuiltContents) {
                            cmds.copyMemoryToAccelerationStructureKHR(deserialization.copyInfo);
                        }
                    }

                    GetVulkanDriver().setMarker(cmds, "After AS build");
                    vk::MemoryBarrier barrier {
                        .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR | vk::AccessFlagBits::eTransferRead,
                        .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR,
                    };
                    cmds.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, static_cast<vk::DependencyFlags>(0), barrier, {}, {});
                    TracyVkCollect(tracyCtx, cmds);
                }
                cmds.end();
            }
        }
    }, 1);

    if(totalSerializedSize > 0) {
        renderer.getVulkanDriver().performSingleTimeTransferCommands([&](vk::CommandBuffer& cmds) {
            serializedASStorageStaging.view.cmdCopyTo(cmds, serializedASStorage.view);
        }, false, {}, {}, *serializedCopySemaphore[semaphoreIndex]);
    }

    {
        ZoneScopedN("Submit BLAS build commands");
        std::vector<vk::CommandBufferSubmitInfo> commandBufferInfos{};
        commandBufferInfos.reserve(buckets.size());
        for(auto& bucket : buckets) {
            if(!bucket.buildInfos.empty() || !bucket.prebuiltContents.empty()) {
                commandBufferInfos.emplace_back(vk::CommandBufferSubmitInfo {
                    .commandBuffer = bucket.cmds,
                });
            }
        }
        Carrot::Vector<vk::SemaphoreSubmitInfo> waitSemaphoreList;
        waitSemaphoreList.setGrowthFactor(2);

        if(totalSerializedSize > 0) {
            waitSemaphoreList.emplaceBack(vk::SemaphoreSubmitInfo {
                .semaphore = *serializedCopySemaphore[semaphoreIndex],
                .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
            });
        }

        // TODO: reuse memory
        std::unordered_set<vk::Semaphore> alreadyWaitedOn;
        for(auto& blas : toBuild) {
            vk::Semaphore boundSemaphore = blas->getBoundSemaphore(renderContext.frameCount);
            if(boundSemaphore != VK_NULL_HANDLE) {
                auto [_, isNew] = alreadyWaitedOn.emplace(boundSemaphore);
                if (isNew) {
                    vk::SemaphoreSubmitInfo& submitInfo = waitSemaphoreList.emplaceBack();
                    submitInfo.semaphore = boundSemaphore;
                    submitInfo.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
                }
            }
        }
        vk::SemaphoreSubmitInfo signalInfo {
            .semaphore = hasASToCompact ? (*preCompactBLASSemaphore[semaphoreIndex]) : (*blasBuildSemaphore[semaphoreIndex]),
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        };
        GetVulkanDriver().submitCompute(vk::SubmitInfo2 {
            .waitSemaphoreInfoCount = static_cast<std::uint32_t>(waitSemaphoreList.size()),
            .pWaitSemaphoreInfos = waitSemaphoreList.data(),

            .commandBufferInfoCount = static_cast<uint32_t>(commandBufferInfos.size()),
            .pCommandBufferInfos = commandBufferInfos.data(),

            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalInfo,
        });
        builtBLASThisFrame = true;
    }

    /*
    if(false) {
        ZoneScopedN("Compress built Acceleration Structures");

        std::vector<vk::DeviceSize> compactSizes = device.getQueryPoolResults<vk::DeviceSize>(*queryPool, 0, blasCount, blasCount*sizeof(vk::DeviceSize), sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait).value;
        std::vector<std::unique_ptr<AccelerationStructure>> compactedASes{};
        for (size_t index = 0; index < blasCount; index++) {
            auto& cmds = compactCommands[index];
            cmds->begin(vk::CommandBufferBeginInfo{
                    .pInheritanceInfo = nullptr,
            });

            // create new AS
            vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = compactSizes[index],
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
            };

            auto compactedAS = std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo);

            // copy AS
            cmds->copyAccelerationStructureKHR(vk::CopyAccelerationStructureInfoKHR {
                .src = toBuild[index]->as->getVulkanAS(),
                .dst = compactedAS->getVulkanAS(),
                .mode = vk::CopyAccelerationStructureModeKHR::eCompact
            });

            compactedASes.emplace_back(std::move(compactedAS));
            cmds->end();
        }

        vk::PipelineStageFlags2 waitStage = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR;
        std::vector<vk::CommandBufferSubmitInfo> commandBufferInfos{};
        commandBufferInfos.reserve(compactCommands.size());
        for(auto& pBuffer : compactCommands) {
            commandBufferInfos.emplace_back(vk::CommandBufferSubmitInfo {
                .commandBuffer = *pBuffer,
            });
        }
        vk::SemaphoreSubmitInfo waitInfo {
            .semaphore = (*preCompactBLASSemaphore[semaphoreIndex]),
            .stageMask = waitStage,
        };
        vk::SemaphoreSubmitInfo signalInfo {
            .semaphore = (*blasBuildSemaphore[semaphoreIndex]),
            .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
        };
        GetVulkanDriver().submitCompute(vk::SubmitInfo2 {
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitInfo,

            .commandBufferInfoCount = static_cast<uint32_t>(toBuild.size()),
            .pCommandBufferInfos = commandBufferInfos.data(),

            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalInfo,
        });
        // replace old AS with compacted AS
        for(size_t i = 0; i < blasCount; i++) {
            toBuild[i]->as = std::move(compactedASes[i]);
        }
    }
    */
}

Carrot::BufferView Carrot::ASBuilder::getIdentityMatrixBufferView() const {
    return identityMatrixForBLASes.view;
}

void Carrot::ASBuilder::buildTopLevelAS(const Carrot::Render::Context& renderContext, bool update) {
    ScopedMarker("ASBuilder::buildTopLevelAS");
    if(!enabled)
        return;
 //   Carrot::Log::debug("buildTopLevelAS %llu", renderContext.frameCount);
    const std::size_t semaphoreIndex = renderContext.frameCount % tlasBuildCommands.size();
    static int prevPrimitiveCount = 0;
    ZoneValue(update ? 1 : 0);

    auto& device = renderer.getLogicalDevice();
    const vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

    std::vector<vk::AccelerationStructureInstanceKHR> vkInstances{};
    std::vector<SceneDescription::Instance> logicalInstances {};
    vkInstances.reserve(instances.getStorageSize());
    logicalInstances.reserve(instances.getStorageSize());

    {
        ZoneScopedN("Convert to vulkan instances");
        instances.iterate([&](std::shared_ptr<InstanceHandle> instance) {
            if(instance->isUsable()) {
                vkInstances.push_back(instance->instance);

                logicalInstances.emplace_back(SceneDescription::Instance {
                    .instanceColor = instance->instanceColor,
                    .firstGeometryIndex = instance->geometry->firstGeometryIndex,
                });
            }
        });
        vkInstances.shrink_to_fit();
    }

    if(vkInstances.empty()) {
        currentTLAS = nullptr;
        instancesBuffer = nullptr;
        return;
    }

    vk::CommandBuffer& buildCommand = *tlasBuildCommands[renderContext.swapchainIndex];

    buildCommand.reset();
    buildCommand.begin(vk::CommandBufferBeginInfo {
    });
    std::uint64_t tlasValueToWait = 0;
    {
        TracyVkZone(tlasBuildTracyCtx[renderContext.swapchainIndex], buildCommand, "TLAS build or update");

        GetVulkanDriver().setFormattedMarker(buildCommand, "Start of command buffer for TLAS build, update = %d, framesBeforeRebuildingTLAS = %d", update, framesBeforeRebuildingTLAS);
        // upload instances to the device

        lastFrameIndexForTLAS = renderContext.swapchainIndex;
        vk::DeviceAddress instanceBufferAddress;
        /*if(!rtInstancesBuffer || lastInstanceCount < vkInstances.size()) */{
            ZoneScopedN("Create RT instances buffer");
            rtInstancesBuffer = std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(
                                                       vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR),
                                                       vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR
            )));

            rtInstancesBuffer->name("TLAS Instances");
            lastInstanceCount = vkInstances.size();

            instanceBufferAddress = rtInstancesBuffer->view.getDeviceAddress();
        }

        /*if(!instancesBuffer || instancesBuffer->getSize() < sizeof(SceneDescription::Instance) * logicalInstances.size()) */{
            ZoneScopedN("Create logical instances buffer");
            instancesBuffer = std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(sizeof(SceneDescription::Instance) * logicalInstances.size(),
                                                                                                            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)));
            instancesBuffer->name(Carrot::sprintf("RT Instances frame %lu", GetRenderer().getFrameCount()));
        }

        {
            {
                ZoneScopedN("Logical instances upload");
                //instancesBuffers[lastFrameIndexForTLAS]->directUpload(logicalInstances.data(), logicalInstances.size() * sizeof(SceneDescription::Instance));
                instancesBuffer->view.uploadForFrame(logicalInstances.data(), logicalInstances.size() * sizeof(SceneDescription::Instance));
            }
            {
                ZoneScopedN("RT Instances upload");
                rtInstancesBuffer->view.stageUpload(*instanceUploadSemaphore[semaphoreIndex], vkInstances.data(), vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
            }
        }

        vk::AccelerationStructureGeometryInstancesDataKHR instancesVk {
            .arrayOfPointers = false,
        };
        instancesVk.data.deviceAddress = instanceBufferAddress;

        vk::AccelerationStructureGeometryKHR topASGeometry {
            .geometryType = vk::GeometryTypeKHR::eInstances,
        };
        topASGeometry.geometry.instances = instancesVk;

        // query size for tlas and scratch space
        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo {
            .flags = flags,
            .mode = update ? vk::BuildAccelerationStructureModeKHR::eUpdate : vk::BuildAccelerationStructureModeKHR::eBuild,
            .srcAccelerationStructure = update ? currentTLAS->getVulkanAS() : nullptr,
            .geometryCount = 1,
            .pGeometries = &topASGeometry,
        };
        size_t count = vkInstances.size();
        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = renderer.getLogicalDevice().getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, count);

        if(!update) {
            vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = sizeInfo.accelerationStructureSize,
                .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        };
            currentTLAS = std::make_shared<AccelerationStructure>(renderer.getVulkanDriver(), createInfo);
        } else {
            verify(renderContext.frameCount > 0, "Updating on first frame??");
            tlasValueToWait = renderContext.frameCount-1;

            ZoneScopedN("Render thread dependency");
            renderer.waitForRenderToComplete(); // this creates a dependency between render thread and main thread
            // due to timing, it is possible that the TLAS build is submitted before anything about the previous frame has been submitted,
            // and (weirdly) this can create deadlocks when trying to present the frame.
            // This happens almost 100% when reloading shaders, for some reason.
        }

        // Allocate scratch memory
        vk::DeviceAddress scratchBufferAddress;
        /*if(!scratchBuffer || lastScratchSize < sizeInfo.buildScratchSize) */{
            rtInstancesScratchBuffer =std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(
                                                     update ? sizeInfo.updateScratchSize : sizeInfo.buildScratchSize,
                                                     vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer
            )));
            rtInstancesScratchBuffer->name(Carrot::sprintf("RT instances build scratch buffer frame %llu", renderContext.frameCount));
            scratchBufferAddress = rtInstancesScratchBuffer->view.getDeviceAddress();
        }

        buildInfo.dstAccelerationStructure = currentTLAS->getVulkanAS();
        buildInfo.scratchData.deviceAddress = scratchBufferAddress;

        // one build offset per instance
        vk::AccelerationStructureBuildRangeInfoKHR buildOffsetInfo {
            .primitiveCount = static_cast<uint32_t>(count)
        };

        GetVulkanDriver().setFormattedMarker(buildCommand, "Before TLAS build, frame = %llu, update = %d, framesBeforeRebuildingTLAS = %d, instanceCount = %llu, prevInstanceCount = %llu", (std::uint64_t)renderer.getFrameCount(), update, framesBeforeRebuildingTLAS, vkInstances.size(), (std::size_t)prevPrimitiveCount);
        buildCommand.buildAccelerationStructuresKHR(buildInfo, &buildOffsetInfo);
        GetVulkanDriver().setFormattedMarker(buildCommand, "After TLAS build, update = %d, framesBeforeRebuildingTLAS = %d, instanceCount = %llu, prevInstanceCount = %llu", update, framesBeforeRebuildingTLAS, vkInstances.size(), (std::size_t)prevPrimitiveCount);

        TracyVkCollect(tlasBuildTracyCtx[renderContext.swapchainIndex], buildCommand);
    }
    buildCommand.end();

    std::vector<vk::SemaphoreSubmitInfo> waitSemaphores;

    waitSemaphores.emplace_back(vk::SemaphoreSubmitInfo {
        .semaphore = *instanceUploadSemaphore[semaphoreIndex],
        .stageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
    });
    waitSemaphores.emplace_back(vk::SemaphoreSubmitInfo {
        .semaphore = *tlasBuildTimelineSemaphore,
        .value = tlasValueToWait,
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    });
    if(builtBLASThisFrame) {
        waitSemaphores.emplace_back(vk::SemaphoreSubmitInfo {
            .semaphore = *blasBuildSemaphore[semaphoreIndex],
            .stageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        });
    }

    vk::CommandBufferSubmitInfo buildCommandInfo {
        .commandBuffer = buildCommand,
    };
    vk::SemaphoreSubmitInfo signalInfo {
        .semaphore = (*tlasBuildSemaphore[semaphoreIndex]),
        .stageMask = vk::PipelineStageFlagBits2::eAllCommands,
    };
    GetVulkanDriver().submitCompute(vk::SubmitInfo2 {
        .waitSemaphoreInfoCount = static_cast<uint32_t>(waitSemaphores.size()),
        .pWaitSemaphoreInfos = waitSemaphores.data(),

        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &buildCommandInfo,

        .signalSemaphoreInfoCount = 1,
        .pSignalSemaphoreInfos = &signalInfo,
    });

    GetEngine().addWaitSemaphoreBeforeRendering(renderContext, vk::PipelineStageFlagBits::eAllCommands, *tlasBuildSemaphore[semaphoreIndex]);

    prevPrimitiveCount = vkInstances.size();
}

void Carrot::ASBuilder::startFrame() {
    if(!enabled)
        return;
}

void Carrot::ASBuilder::waitForCompletion(vk::CommandBuffer& cmds) {
    if(!enabled)
        return;
}

Carrot::AccelerationStructure* Carrot::ASBuilder::getTopLevelAS(const Carrot::Render::Context& renderContext) {
    verify(GetCapabilities().supportsRaytracing, "Raytracing is not supported");
    return tlasPerFrame[renderContext.swapchainIndex].get();
}

void Carrot::ASBuilder::onSwapchainImageCountChange(size_t newCount) {
    createSemaphores();
    createBuildCommandBuffers();
    createQueryPools();
    createGraveyard();
    createDescriptors();

    rtInstancesBufferPerFrame.clear();
    rtInstancesBufferPerFrame.resize(newCount);

    rtInstancesScratchBufferPerFrame.clear();
    rtInstancesScratchBufferPerFrame.resize(newCount);

    instancesBufferPerFrame.clear();
    instancesBufferPerFrame.resize(newCount);

    geometriesBufferPerFrame.clear();
    geometriesBufferPerFrame.resize(newCount);

    tlasPerFrame.clear();
    tlasPerFrame.resize(newCount);

    blasBuildCommandObjects.resize(newCount);
    prebuiltBLASStorages.resize(newCount);
    tlasBuildTimelineSemaphoreWaitValuesForRenderThread.resize(newCount);
    for(std::size_t i = 0; i < newCount; i++) {
        tlasBuildTimelineSemaphoreWaitValuesForRenderThread[i] = 0;
    }
}

void Carrot::ASBuilder::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {

}

Carrot::BufferView Carrot::ASBuilder::getGeometriesBuffer(const Render::Context& renderContext) {
    if(!geometriesBuffer) {
        return Carrot::BufferView{};
    }
    return geometriesBufferPerFrame[renderContext.swapchainIndex]->view;
}

Carrot::BufferView Carrot::ASBuilder::getInstancesBuffer(const Render::Context& renderContext) {
    return instancesBufferPerFrame[renderContext.swapchainIndex]->view;
}

vk::Semaphore Carrot::ASBuilder::getTlasBuildTimelineSemaphore() const {
    return *tlasBuildTimelineSemaphore;
}

std::uint64_t Carrot::ASBuilder::getTlasBuildTimelineSemaphoreSignalValue(const Render::Context& renderContext) const {
    return renderContext.frameCount;
}

/*static*/ vk::TransformMatrixKHR Carrot::ASBuilder::glmToRTTransformMatrix(const glm::mat4& mat) {
    vk::TransformMatrixKHR rtTransform;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 3; ++row) {
            rtTransform.matrix[row][column] = mat[column][row];
        }
    }
    return rtTransform;
}
