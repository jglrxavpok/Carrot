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
#include "RayTracer.h"
#include "engine/render/resources/ResourceAllocator.h"

namespace Carrot {
    static constexpr glm::mat4 IdentityMatrix = glm::identity<glm::mat4>();
    static constexpr std::size_t BLASBuildBucketCount = 32;

    BLASHandle::BLASHandle(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                           const std::vector<glm::mat4>& transforms,
                           const std::vector<std::uint32_t>& materialSlots,
                           BLASGeometryFormat geometryFormat,
                           ASBuilder* builder):
    meshes(meshes), materialSlots(materialSlots), builder(builder), geometryFormat(geometryFormat) {
        ZoneScoped;

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
                           ASBuilder* builder):
    meshes(meshes), materialSlots(materialSlots), builder(builder), geometryFormat(geometryFormat) {
        innerInit(transforms);
    }

    void BLASHandle::innerInit(std::span<const vk::DeviceAddress/*vk::TransformMatrixKHR*/> transformAddresses) {
        ZoneScoped;
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
        return boundSemaphores[swapchainIndex];
    }

    InstanceHandle::InstanceHandle(std::weak_ptr<BLASHandle> geometry, ASBuilder* builder):
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

        auto geometryPtr = geometry.lock();
        verify(geometryPtr, "geometry == nullptr, did it get released when it should not have?");

        vk::AccelerationStructureDeviceAddressInfoKHR addressInfo {
                .accelerationStructure = geometryPtr->as->getVulkanAS()
        };
        auto blasAddress = GetRenderer().getLogicalDevice().getAccelerationStructureAddressKHR(addressInfo);

        setAndCheck(instance.accelerationStructureReference, blasAddress);
    }
}

// --

Carrot::ASBuilder::ASBuilder(Carrot::VulkanRenderer& renderer): renderer(renderer) {
    enabled = GetCapabilities().supportsRaytracing;

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

void Carrot::ASBuilder::createBuildCommandBuffers() {
    tlasBuildCommands = GetVulkanDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
            .commandPool = renderer.getVulkanDriver().getThreadComputeCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = GetEngine().getSwapchainImageCount(),
    });

    blasBuildTracyCtx.resize(GetEngine().getSwapchainImageCount());
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
                                                                      BLASGeometryFormat geometryFormat) {
    ZoneScoped;
    if(!enabled)
        return nullptr;

    //access.lock();
    ASStorage<BLASHandle>::Reservation slot = staticGeometries.reserveSlot();
    //access.unlock();
    {
        ZoneScopedN("Create BLASHandle");
        std::shared_ptr<BLASHandle> ptr = std::make_shared<BLASHandle>(/*slot.index, [this](WeakPoolHandle* element) {
                  //  staticGeometries.freeSlot(element->getSlot());
        },*/ meshes, transformAddresses, materials, geometryFormat, this);
        //std::shared_ptr<BLASHandle> ptr = nullptr;
        *slot = ptr;
        return ptr;
    }
}

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                                                                      const std::vector<glm::mat4>& transforms,
                                                                      const std::vector<std::uint32_t>& materials,
                                                                      BLASGeometryFormat geometryFormat) {
    ZoneScoped;
    if(!enabled)
        return nullptr;
    //access.lock();
    ASStorage<BLASHandle>::Reservation slot = staticGeometries.reserveSlot();
    //access.unlock();
    auto ptr = std::make_shared<BLASHandle>(/*slot.index, [this](WeakPoolHandle* element) {
              //  staticGeometries.freeSlot(element->getSlot());
    },*/ meshes, transforms, materials, geometryFormat, this);
    *slot = ptr;
    return ptr;
}

std::shared_ptr<Carrot::InstanceHandle> Carrot::ASBuilder::addInstance(std::weak_ptr<Carrot::BLASHandle> correspondingGeometry) {
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
    geometryUploadSemaphore.resize(imageCount);
    instanceUploadSemaphore.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        preCompactBLASSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        blasBuildSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        tlasBuildSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        geometryUploadSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        instanceUploadSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
    }
}

void Carrot::ASBuilder::onFrame(const Carrot::Render::Context& renderContext) {
    if(!enabled)
        return;
    if(renderContext.pViewport != &GetEngine().getMainViewport()) {
        return;
    }
    ScopedMarker("ASBuilder::onFrame");

    std::vector<std::shared_ptr<BLASHandle>> toBuild;
    staticGeometries.iterate([&](std::shared_ptr<BLASHandle> pValue) {
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
    }
    dirtyBlases = false;

    std::size_t activeInstances = 0;

    instances.iterate([&](std::shared_ptr<InstanceHandle> value) {
        if(value->isUsable()) {
            value->update();
            activeInstances++;
        }
    });

    bool requireTLASRebuildNow = !toBuild.empty() || dirtyInstances || activeInstances > previousActiveInstances;
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

    tlasPerFrame[renderContext.swapchainIndex] = currentTLAS;
}

void Carrot::ASBuilder::resetBlasBuildCommands(const Carrot::Render::Context& renderContext) {
    auto& perThreadCommandPools = blasBuildCommandPool[renderContext.swapchainIndex];
    if(!perThreadCommandPools) {
        perThreadCommandPools = std::make_unique<Carrot::Async::ParallelMap<std::thread::id, vk::UniqueCommandPool>>();
    }
    for(auto& [_, ppPool] : perThreadCommandPools->snapshot()) {
        renderer.getLogicalDevice().resetCommandPool(*(*ppPool));
    }
}


vk::CommandBuffer Carrot::ASBuilder::getBlasBuildCommandBuffer(const Carrot::Render::Context& renderContext) {
    auto& perThreadCommandPools = blasBuildCommandPool[renderContext.swapchainIndex];
    std::thread::id currentThreadID = std::this_thread::get_id();
    vk::CommandPool& commandPool = *perThreadCommandPools->getOrCompute(currentThreadID, [&]() {
        return renderer.getVulkanDriver().createComputeCommandPool();
    });
    auto result = renderer.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
        .commandPool = commandPool,
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

    // add a bottom level AS for each geometry entry
    std::size_t blasCount = toBuild.size();
    if(blasCount == 0) {
        return;
    }

    resetBlasBuildCommands(renderContext);

    struct BLASBucket {
        Cider::Mutex access;
        vk::CommandBuffer cmds; // where to register build commands for this bucket

        Carrot::Vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfos;
        Carrot::Vector<const vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRanges;
    };

    // TODO: reuse memory
    Carrot::Vector<BLASBucket> buckets;
    Carrot::Vector<std::uint32_t> blas2Bucket;
    Carrot::Vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfo;
    Carrot::Vector<vk::DeviceSize> allocationSizes;
    Carrot::Vector<Carrot::BufferAllocation> asMemoryList;

    buckets.resize(BLASBuildBucketCount);

    blas2Bucket.resize(blasCount);
    buildInfo.resize(blasCount);
    allocationSizes.resize(blasCount);
    asMemoryList.resize(blasCount);

    for(std::size_t blasIndex = 0; blasIndex < blasCount; blasIndex++) {
        blas2Bucket[blasIndex] = (blasIndex / BLASBuildBucketCount) % BLASBuildBucketCount;
    }

    auto& tracyCtxList = blasBuildTracyCtx[renderContext.swapchainIndex];
    vk::UniqueQueryPool& queryPool = queryPools[renderContext.swapchainIndex];

    std::atomic_bool hasNewGeometry{ false };

    for (std::size_t bucketIndex = 0; bucketIndex < BLASBuildBucketCount; bucketIndex++) {
        auto& bucket = buckets[bucketIndex];
        bucket.buildInfos.setGrowthFactor(2);
        bucket.pBuildRanges.setGrowthFactor(2);
    }
/*TODO: queries
       if(buildCommands.size() < blasCount) {
        vk::QueryPoolCreateInfo queryPoolCreateInfo{
                .queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
                .queryCount = static_cast<uint32_t>(BucketCount),
        };

        queryPool = GetVulkanDevice().createQueryPoolUnique(queryPoolCreateInfo, GetVulkanDriver().getAllocationCallbacks());
    }*/


    std::atomic<vk::DeviceSize> scratchSize = 0;

    // TODO: fast build for virtual geometry
    auto getBuildFlags = [](BLASHandle& blas) {
        return blas.dynamicGeometry ?
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate) :
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction);
    };

    // allocate memory for each entry
    std::vector<bool> firstBuild;
    std::vector<vk::DeviceSize> scratchOffset;
    firstBuild.resize(blasCount);
    scratchOffset.resize(blasCount);

    std::size_t newGeometriesCount = 0;
    for(size_t index = 0; index < blasCount; index++) {
        auto& blas = *toBuild[index];
        firstBuild[index] = dirtyBlases || blas.firstGeometryIndex == (std::uint32_t) -1;
        if (firstBuild[index]) {
            newGeometriesCount += blas.geometries.size();
        }
    }

    // resize + threads + atomic increment => allGeometries is guaranteed to already have the necessary allocation
    std::atomic_uint32_t geometryIndex = allGeometries.size();
    allGeometries.resize(allGeometries.size() + newGeometriesCount);

    //for(size_t index = 0; index < blasCount; index++) {
    GetTaskScheduler().parallelFor(blasCount, [&](std::size_t index) {
        ScopedMarker("Prepare BLASes");
        //ZoneValue(index);

        // TODO: can probably be cached
        auto& info = buildInfo[index];
        auto& blas = *toBuild[index];

        const vk::BuildAccelerationStructureFlagsKHR flags = getBuildFlags(blas);

        info.geometryCount = blas.geometries.size();
        info.flags = flags;
        info.pGeometries = blas.geometries.data();

        if (firstBuild[index]) {
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
        }

        info.mode = firstBuild[index] ? vk::BuildAccelerationStructureModeKHR::eBuild : vk::BuildAccelerationStructureModeKHR::eUpdate;
        info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        info.srcAccelerationStructure = firstBuild[index] ? nullptr : blas.as->getVulkanAS();

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

        allocationSizes[index] = sizeInfo.accelerationStructureSize;
        scratchOffset[index] = scratchSize.fetch_add(firstBuild[index] ? sizeInfo.buildScratchSize : sizeInfo.updateScratchSize);

        //}
    }, static_cast<std::size_t>(ceil(static_cast<double>(blasCount) / BLASBuildBucketCount)));

    {
        ZoneScopedN("Allocate memory for BLASes");
        GetResourceAllocator().multiAllocateDeviceBuffer(asMemoryList, allocationSizes, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);
    }

    GetTaskScheduler().parallelFor(blasCount, [&](std::size_t index) {
        ZoneScopedN("Create BLASes");
        ZoneValue(index);

        auto& info = buildInfo[index];
        auto& blas = *toBuild[index];

        vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = allocationSizes[index],
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        };

        blas.as = std::move(std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo, std::move(asMemoryList[index])));
        info.dstAccelerationStructure = blas.as->getVulkanAS();
        //}
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
    Buffer blasBuildScratchBuffer = Buffer(renderer.getVulkanDriver(), scratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal, std::set{queueFamilies.computeFamily.value()});
    blasBuildScratchBuffer.name("BLAS build scratch buffer");

    bool hasASToCompact = false;
    auto scratchAddress = device.getBufferAddress({.buffer = blasBuildScratchBuffer.getVulkanBuffer() });
    GetTaskScheduler().parallelFor(BLASBuildBucketCount, [&](std::size_t bucketIndex) {
    //for(std::size_t index = 0; index < blasCount; index++) {
        ZoneScopedN("Process bucket");
        ZoneValue(bucketIndex);
        {
            ZoneScopedN("Put BLASes in bucket");

            BLASBucket& bucket = buckets[bucketIndex];
            for(std::size_t blasStartIndex = bucketIndex * BLASBuildBucketCount; blasStartIndex < blasCount; blasStartIndex += BLASBuildBucketCount*BLASBuildBucketCount) {
                for(std::size_t blasIndex = blasStartIndex; blasIndex < blasStartIndex + BLASBuildBucketCount && blasIndex < blasCount; blasIndex++) {
                    buildInfo[blasIndex].scratchData.deviceAddress = scratchAddress + scratchOffset[blasIndex];

                    bucket.buildInfos.emplaceBack(buildInfo[blasIndex]);
                    bucket.pBuildRanges.emplaceBack(toBuild[blasIndex]->buildRanges.data());
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
            BLASBucket& bucket = buckets[bucketIndex];
            if(!bucket.buildInfos.empty()) {
                auto& cmds = bucket.cmds;
                cmds = getBlasBuildCommandBuffer(renderContext);
                cmds.begin(vk::CommandBufferBeginInfo {
                    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                    .pInheritanceInfo = nullptr,
                });
                //TracyVkZone(tracyCtxList[index], *cmds, "Build BLAS");

                GetVulkanDriver().setMarker(cmds, "Begin AS build");

                {
                    ZoneScopedN("Record AS build");
                    cmds.buildAccelerationStructuresKHR(bucket.buildInfos, bucket.pBuildRanges);
                }

                GetVulkanDriver().setMarker(cmds, "After AS build");
                vk::MemoryBarrier barrier {
                    .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
                    .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR,
                };
                cmds.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, static_cast<vk::DependencyFlags>(0), barrier, {}, {});
                cmds.end();
                //TracyVkCollect(tracyCtxList[index], *cmds);
            }
        }
    }, 1);


    {
        ZoneScopedN("Submit BLAS build commands");
        std::vector<vk::CommandBufferSubmitInfo> commandBufferInfos{};
        commandBufferInfos.reserve(buckets.size());
        for(auto& bucket : buckets) {
            if(!bucket.buildInfos.empty()) {
                commandBufferInfos.emplace_back(vk::CommandBufferSubmitInfo {
                    .commandBuffer = bucket.cmds,
                });
            }
        }
        Carrot::Vector<vk::SemaphoreSubmitInfo> waitSemaphoreList;
        waitSemaphoreList.setGrowthFactor(2);

        for(auto& blas : toBuild) {
            // TODO: deduplicate semaphores inside list? Is it even useful?
            vk::Semaphore boundSemaphore = blas->getBoundSemaphore(renderContext.swapchainIndex);
            if(boundSemaphore != VK_NULL_HANDLE) {
                vk::SemaphoreSubmitInfo& submitInfo = waitSemaphoreList.emplaceBack();
                submitInfo.semaphore = boundSemaphore;
                submitInfo.stageMask = vk::PipelineStageFlagBits2::eAllCommands;
            }
        }
        vk::SemaphoreSubmitInfo signalInfo {
            .semaphore = hasASToCompact ? (*preCompactBLASSemaphore[renderContext.swapchainIndex]) : (*blasBuildSemaphore[renderContext.swapchainIndex]),
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
        GetEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eTopOfPipe, *blasBuildSemaphore[renderContext.swapchainIndex]);
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
            .semaphore = (*preCompactBLASSemaphore[renderContext.swapchainIndex]),
            .stageMask = waitStage,
        };
        vk::SemaphoreSubmitInfo signalInfo {
            .semaphore = (*blasBuildSemaphore[renderContext.swapchainIndex]),
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
    if(!enabled)
        return;
    static int prevPrimitiveCount = 0;
    ScopedMarker("ASBuilder::buildTopLevelAS");
    ZoneValue(update ? 1 : 0);

    auto& device = renderer.getLogicalDevice();
    const vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

    std::vector<vk::AccelerationStructureInstanceKHR> vkInstances{};
    std::vector<SceneDescription::Instance> logicalInstances {};
    vkInstances.reserve(instances.getStorageSize());
    logicalInstances.reserve(instances.getStorageSize());

    {
        ZoneScopedN("Convert to vulkan instances");
        instances.iterate([&](std::shared_ptr<InstanceHandle> instance) {
            if(instance->isUsable()) {
                if(auto pGeometry = instance->geometry.lock()) {
                    vkInstances.push_back(instance->instance);

                    logicalInstances.emplace_back(SceneDescription::Instance {
                        .instanceColor = instance->instanceColor,
                        .firstGeometryIndex = pGeometry->firstGeometryIndex,
                    });
                }
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

    buildCommand.begin(vk::CommandBufferBeginInfo {
    });
    GetVulkanDriver().setFormattedMarker(buildCommand, "Start of command buffer for TLAS build, update = %d, framesBeforeRebuildingTLAS = %d", update, framesBeforeRebuildingTLAS);
    // upload instances to the device

    lastFrameIndexForTLAS = renderContext.swapchainIndex;
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
            rtInstancesBuffer->view.stageUpload(*instanceUploadSemaphore[renderContext.swapchainIndex], vkInstances.data(), vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
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
    }

    // Allocate scratch memory
    /*if(!scratchBuffer || lastScratchSize < sizeInfo.buildScratchSize) */{
        rtInstancesScratchBuffer =std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(
                                                 update ? sizeInfo.updateScratchSize : sizeInfo.buildScratchSize,
                                                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer
        )));
        rtInstancesScratchBuffer->name("RT instances build scratch buffer");
        lastScratchSize = rtInstancesScratchBuffer->view.getSize();
        scratchBufferAddress = rtInstancesScratchBuffer->view.getDeviceAddress();
    }

    buildInfo.dstAccelerationStructure = currentTLAS->getVulkanAS();
    buildInfo.scratchData.deviceAddress = scratchBufferAddress;

    // one build offset per instance
    vk::AccelerationStructureBuildRangeInfoKHR buildOffsetInfo {
        .primitiveCount = static_cast<uint32_t>(vkInstances.size())
    };

    GetVulkanDriver().setFormattedMarker(buildCommand, "Before TLAS build, update = %d, framesBeforeRebuildingTLAS = %d, instanceCount = %llu, prevInstanceCount = %llu", update, framesBeforeRebuildingTLAS, vkInstances.size(), (std::size_t)prevPrimitiveCount);
    buildCommand.buildAccelerationStructuresKHR(buildInfo, &buildOffsetInfo);
    GetVulkanDriver().setFormattedMarker(buildCommand, "After TLAS build, update = %d, framesBeforeRebuildingTLAS = %d, instanceCount = %llu, prevInstanceCount = %llu", update, framesBeforeRebuildingTLAS, vkInstances.size(), (std::size_t)prevPrimitiveCount);
    buildCommand.end();

    std::vector<vk::SemaphoreSubmitInfo> waitSemaphores;

    waitSemaphores.emplace_back(vk::SemaphoreSubmitInfo {
        .semaphore = *instanceUploadSemaphore[renderContext.swapchainIndex],
        .stageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
    });
    if(builtBLASThisFrame) {
        waitSemaphores.emplace_back(vk::SemaphoreSubmitInfo {
            .semaphore = *blasBuildSemaphore[renderContext.swapchainIndex],
            .stageMask = vk::PipelineStageFlagBits2::eAccelerationStructureBuildKHR,
        });
    }

    vk::CommandBufferSubmitInfo buildCommandInfo {
        .commandBuffer = buildCommand,
    };
    vk::SemaphoreSubmitInfo signalInfo {
        .semaphore = (*tlasBuildSemaphore[renderContext.swapchainIndex]),
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

    GetEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eFragmentShader, *tlasBuildSemaphore[renderContext.swapchainIndex]);

    bottomLevelBarriers.clear();
    const BufferView& tlasBufferView = currentTLAS->getBuffer().view;
    topLevelBarriers.push_back(vk::BufferMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuildKHR,
            .srcAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureWriteKHR,
            .dstStageMask = vk::PipelineStageFlagBits2KHR::eAllGraphics,
            .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureReadKHR,

            .srcQueueFamilyIndex = 0,
            .dstQueueFamilyIndex = 0,

            .buffer = tlasBufferView.getVulkanBuffer(),
            .offset = tlasBufferView.getStart(),
            .size = tlasBufferView.getSize(),
    });

    prevPrimitiveCount = vkInstances.size();
}

void Carrot::ASBuilder::startFrame() {
    if(!enabled)
        return;
    topLevelBarriers.clear();
}

void Carrot::ASBuilder::waitForCompletion(vk::CommandBuffer& cmds) {
    if(!enabled)
        return;
    vk::DependencyInfoKHR dependency {
            .bufferMemoryBarrierCount = static_cast<uint32_t>(topLevelBarriers.size()),
            .pBufferMemoryBarriers = topLevelBarriers.data(),
    };
    cmds.pipelineBarrier2KHR(dependency);
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

    blasBuildCommandPool.resize(newCount);
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

/*static*/ vk::TransformMatrixKHR Carrot::ASBuilder::glmToRTTransformMatrix(const glm::mat4& mat) {
    vk::TransformMatrixKHR rtTransform;
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 3; ++row) {
            rtTransform.matrix[row][column] = mat[column][row];
        }
    }
    return rtTransform;
}
