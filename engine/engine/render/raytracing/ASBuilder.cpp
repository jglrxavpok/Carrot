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
#include <core/io/Logging.hpp>
#include "RayTracer.h"
#include "engine/render/resources/ResourceAllocator.h"

namespace Carrot {
    static constexpr glm::mat4 IdentityMatrix = glm::identity<glm::mat4>();

    BLASHandle::BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor,
                           const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                           const std::vector<glm::mat4>& transforms,
                           const std::vector<std::uint32_t>& materialSlots,
                           BLASGeometryFormat geometryFormat,
                           ASBuilder* builder):
    WeakPoolHandle(index, std::move(destructor)),
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

    BLASHandle::BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor,
                           const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                           const std::vector<vk::DeviceAddress>& transforms,
                           const std::vector<std::uint32_t>& materialSlots,
                           BLASGeometryFormat geometryFormat,
                           ASBuilder* builder):
    WeakPoolHandle(index, std::move(destructor)),
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

        builder->dirtyBlases = true;
    }

    BLASHandle::~BLASHandle() noexcept {
        builder->dirtyBlases = true;
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

    InstanceHandle::InstanceHandle(std::uint32_t index, std::function<void(WeakPoolHandle *)> destructor,
                                   std::weak_ptr<BLASHandle> geometry, ASBuilder* builder):
            WeakPoolHandle(index, destructor), geometry(geometry), builder(builder) {
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
    blasBuildCommands.resize(GetEngine().getSwapchainImageCount());
    compactBLASCommands.resize(GetEngine().getSwapchainImageCount());
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
    access.lock();
    WeakPool<BLASHandle>::Reservation slot = staticGeometries.reserveSlot();
    access.unlock();
    std::shared_ptr<BLASHandle> ptr = std::make_shared<BLASHandle>(slot.index, [this](WeakPoolHandle* element) {
                staticGeometries.freeSlot(element->getSlot());
    }, meshes, transformAddresses, materials, geometryFormat, this);
    //std::shared_ptr<BLASHandle> ptr = nullptr;
    slot.ptr = ptr;
    return ptr;
}

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                                                                      const std::vector<glm::mat4>& transforms,
                                                                      const std::vector<std::uint32_t>& materials,
                                                                      BLASGeometryFormat geometryFormat) {
    ZoneScoped;
    if(!enabled)
        return nullptr;
    access.lock();
    WeakPool<BLASHandle>::Reservation slot = staticGeometries.reserveSlot();
    access.unlock();
    auto ptr = std::make_shared<BLASHandle>(slot.index, [this](WeakPoolHandle* element) {
                staticGeometries.freeSlot(element->getSlot());
    }, meshes, transforms, materials, geometryFormat, this);
    slot.ptr = ptr;
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
    Async::LockGuard l { access };
    auto purge = [](auto& pool) {
        pool.erase(std::find_if(WHOLE_CONTAINER(pool), [](auto a) { return a.second.expired(); }), pool.end());
    };
    purge(instances);
    purge(staticGeometries);

    std::vector<std::shared_ptr<BLASHandle>> toBuild;
    for(auto& [slot, valuePtr] : staticGeometries) {
        if(auto value = valuePtr.lock()) {
            if(!value->isBuilt() || dirtyBlases) {
                toBuild.push_back(value);
                value->built = false;
            }
        }
    }

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

    for(auto& [slot, valuePtr] : instances) {
        if(auto value = valuePtr.lock()) {
            if(value->isUsable()) {
                value->update();
                activeInstances++;
            }
        }
    }

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
    
    const std::size_t maxCommandBuffers = 32;
    const std::size_t elementsPerBuffer = (blasCount+blasCount-1) / maxCommandBuffers;
    auto elementIndexToBuffer = [&](std::size_t elementIndex) {
        return elementIndex / elementsPerBuffer;
    };

    auto& tracyCtxList = blasBuildTracyCtx[renderContext.swapchainIndex];
    auto& buildCommands = blasBuildCommands[renderContext.swapchainIndex];
    auto& compactCommands = compactBLASCommands[renderContext.swapchainIndex];
    vk::UniqueQueryPool& queryPool = queryPools[renderContext.swapchainIndex];

    // TODO: reduce number of command buffers (batch of 32 ?)
    // TODO: fast build
    bool hasNewGeometry = false;
    if(buildCommands.size() < blasCount) {
        ZoneScopedN("Reallocate command buffers");
        buildCommands = GetVulkanDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
                .commandPool = renderer.getVulkanDriver().getThreadComputeCommandPool(),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = static_cast<std::uint32_t>(blasCount),
        });

        tracyCtxList.resize(blasCount);
        for(std::size_t i = 0; i < tracyCtxList.size(); i++) {
            if(!tracyCtxList[i]) {
               // tracyCtxList[i] = TracyVkContext(GetVulkanDriver().getPhysicalDevice(), GetVulkanDevice(), GetVulkanDriver().getComputeQueue().getQueueUnsafe(), *buildCommands[i]);
                const std::string name = Carrot::sprintf("Blas build swapchainIndex %d index %llu", renderContext.swapchainIndex, i);
               // TracyVkContextName(tracyCtxList[i], name.c_str(), name.size());
            }
        }

        compactCommands = GetVulkanDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
                .commandPool = renderer.getVulkanDriver().getThreadComputeCommandPool(),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = static_cast<std::uint32_t>(blasCount),
        });

        vk::QueryPoolCreateInfo queryPoolCreateInfo{
                .queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
                .queryCount = static_cast<uint32_t>(blasCount),
        };

        queryPool = GetVulkanDevice().createQueryPoolUnique(queryPoolCreateInfo, GetVulkanDriver().getAllocationCallbacks());
    }
    verify(compactCommands.size() == buildCommands.size(), "Compact commands must have as many items as build commands!");

    std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfo{blasCount};

    vk::DeviceSize scratchSize = 0;
    std::vector<vk::DeviceSize> originalSizes(blasCount);

    auto getBuildFlags = [](BLASHandle& blas) {
        return blas.dynamicGeometry ?
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate) :
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction);
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

    if(allGeometries.capacity() < allGeometries.size() + newGeometriesCount) {
        allGeometries.reserve(allGeometries.size() + newGeometriesCount);
    }

    for(size_t index = 0; index < blasCount; index++) {
        ZoneScopedN("Create BLAS");
        ZoneValue(index);

        // TODO: can probably be cached
        auto& info = buildInfo[index];
        auto& blas = *toBuild[index];

        const vk::BuildAccelerationStructureFlagsKHR flags = getBuildFlags(blas);

        info.geometryCount = blas.geometries.size();
        info.flags = flags;
        info.pGeometries = blas.geometries.data();

        if (firstBuild[index]) {
            std::size_t geometryIndexStart = allGeometries.size();
            blas.firstGeometryIndex = geometryIndexStart;
            for (std::size_t j = 0; j < blas.geometries.size(); ++j) {
                allGeometries.emplace_back(SceneDescription::Geometry {
                        .vertexBufferAddress = blas.geometries[j].geometry.triangles.vertexData.deviceAddress,
                        .indexBufferAddress = blas.geometries[j].geometry.triangles.indexData.deviceAddress,
                        .materialIndex = blas.materialSlots[j],
                        .geometryFormat = blas.geometryFormat,
                });
            }
            hasNewGeometry = true;
        }


        info.mode = firstBuild[index] ? vk::BuildAccelerationStructureModeKHR::eBuild : vk::BuildAccelerationStructureModeKHR::eUpdate;
        info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        info.srcAccelerationStructure = firstBuild[index] ? nullptr : blas.as->getVulkanAS();
    }

    Async::SpinLock scratchSizeAccess;
    // There is some weird GPU crashes happening when processing this loop out-of-order
    // TODO: understand why
    for(size_t index = 0; index < blasCount; index++) {
    //GetTaskScheduler().parallelFor(blasCount, [&](std::size_t index) {

        ZoneScopedN("Create BLAS");
        ZoneValue(index);

        // TODO: can probably be cached
        auto& info = buildInfo[index];
        auto& blas = *toBuild[index];

        std::vector<uint32_t> primitiveCounts(blas.buildRanges.size());
        // copy primitive counts to flat vector
        for(size_t geomIndex = 0; geomIndex < primitiveCounts.size(); geomIndex++) {
            primitiveCounts[geomIndex] = blas.buildRanges[geomIndex].primitiveCount;
        }

        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
                device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, info, primitiveCounts);

        vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = sizeInfo.accelerationStructureSize,
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        };

        blas.as = std::move(std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo));
        info.dstAccelerationStructure = blas.as->getVulkanAS();

        {
            Async::LockGuard scratchSizeGuard { scratchSizeAccess };
            originalSizes[index] = sizeInfo.accelerationStructureSize;
            scratchOffset[index] = scratchSize;
            scratchSize += firstBuild[index] ? sizeInfo.buildScratchSize : sizeInfo.updateScratchSize;
        }

        }
    //}, 32);

    if(hasNewGeometry) {
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
    //GetTaskScheduler().parallelFor(blasCount, [&](std::size_t index) {
    for(std::size_t index = 0; index < blasCount; index++) {
        ZoneScopedN("Record BLAS build command buffer");
        ZoneValue(index);

        bool compactAS = (getBuildFlags(*toBuild[index]) & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) == vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
        compactAS = false;
        hasASToCompact |= compactAS;

        // TODO: needs to be in different command pools
        auto& cmds = buildCommands[index];
        cmds->begin(vk::CommandBufferBeginInfo {
                .pInheritanceInfo = nullptr,
        });
        GetVulkanDriver().setMarker(*cmds, "Begin AS build");

        //TracyVkZone(tracyCtxList[index], *cmds, "Build BLAS");

        cmds->resetQueryPool(*queryPool, index, 1);

        buildInfo[index].scratchData.deviceAddress = scratchAddress + scratchOffset[index];

        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> pBuildRanges{toBuild[index]->buildRanges.size()};
        for(size_t i = 0; i < toBuild[index]->buildRanges.size(); i++) {
            pBuildRanges[i] = toBuild[index]->buildRanges[i];
        }

        GetVulkanDriver().setMarker(*cmds, "Before AS build");
        // build AS
        cmds->buildAccelerationStructuresKHR(buildInfo[index], pBuildRanges.data());
        GetVulkanDriver().setMarker(*cmds, "After AS build");
        vk::MemoryBarrier barrier {
            .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
            .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR,
        };
        cmds->pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, static_cast<vk::DependencyFlags>(0), barrier, {}, {});

        // write compacted size
        if(compactAS) {
            cmds->writeAccelerationStructuresPropertiesKHR(toBuild[index]->as->getVulkanAS(), vk::QueryType::eAccelerationStructureCompactedSizeKHR, *queryPool, index);
        }

//TracyVkCollect(tracyCtxList[index], *cmds);
        cmds->end();
    //}, 32);
    }

    {
        ZoneScopedN("Submit BLAS build commands");
        std::vector<vk::CommandBufferSubmitInfo> commandBufferInfos{};
        commandBufferInfos.reserve(buildCommands.size());
        for(auto& pBuffer : buildCommands) {
            commandBufferInfos.emplace_back(vk::CommandBufferSubmitInfo {
                .commandBuffer = *pBuffer,
            });
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

            .commandBufferInfoCount = static_cast<uint32_t>(toBuild.size()),
            .pCommandBufferInfos = commandBufferInfos.data(),

            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalInfo,
        });
        builtBLASThisFrame = true;
        GetEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eTopOfPipe, *blasBuildSemaphore[renderContext.swapchainIndex]);
    }

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
}

Carrot::BufferView Carrot::ASBuilder::getIdentityMatrixBufferView() const {
    return identityMatrixForBLASes.view;
}

std::shared_ptr<Carrot::InstanceHandle> Carrot::ASBuilder::addInstance(std::weak_ptr<Carrot::BLASHandle> correspondingGeometry) {
    if(!enabled)
        return nullptr;
    Async::LockGuard l { access };
    return instances.create(correspondingGeometry, this);
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
    vkInstances.reserve(instances.size());
    logicalInstances.reserve(instances.size());

    {
        ZoneScopedN("Convert to vulkan instances");
        for(const auto& [slot, v] : instances) {
            if(auto instance = v.lock()) {
                if(instance->isUsable()) {
                    if(auto pGeometry = instance->geometry.lock()) {
                        vkInstances.push_back(instance->instance);

                        logicalInstances.emplace_back(SceneDescription::Instance {
                            .instanceColor = instance->instanceColor,
                            .firstGeometryIndex = pGeometry->firstGeometryIndex,
                        });
                    }
                }
            }
        }
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
