//
// Created by jglrxavpok on 30/12/2020.
//

#include "ASBuilder.h"
#include <engine/render/resources/Mesh.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>
#include "RayTracer.h"
#include "engine/render/resources/ResourceAllocator.h"

namespace Carrot {
    BLASHandle::BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor,
                           const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                           const std::vector<glm::mat4>& transforms,
                           const std::vector<std::uint32_t>& materialSlots,
                           ASBuilder* builder):
    WeakPoolHandle(index, std::move(destructor)),
    meshes(meshes), materialSlots(materialSlots), builder(builder) {
        geometries.reserve(meshes.size());
        buildRanges.reserve(meshes.size());

        verify(meshes.size() == transforms.size(), "Need as many meshes as there are transforms!");
        verify(meshes.size() == materialSlots.size(), "Need as many material handles as there are meshes!");

        transformData = GetResourceAllocator().allocateDedicatedBuffer(
                transforms.size() * sizeof(vk::TransformMatrixKHR),
                vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                vk::MemoryPropertyFlagBits::eDeviceLocal);

        std::vector<vk::TransformMatrixKHR> rtTransforms;
        rtTransforms.resize(transforms.size());

        for (int j = 0; j < transforms.size(); ++j) {
            for (int column = 0; column < 4; ++column) {
                for (int row = 0; row < 3; ++row) {
                    rtTransforms[j].matrix[row][column] = transforms[j][column][row];
                }
            }
        }

        // TODO: don't wait for upload to complete?
        transformData->stageUploadWithOffset(0, rtTransforms.data(), rtTransforms.size() * sizeof(vk::TransformMatrixKHR));

        for(std::size_t i = 0; i < meshes.size(); i++) {
            auto& meshPtr = meshes[i];
            auto& transform = transforms[i];
            verify(meshPtr->getSizeOfSingleVertex() == sizeof(Carrot::Vertex), "Only Carrot::Vertex structure is supported for the moment");
            // TODO: support SkinnedVertex format

            auto& device = GetRenderer().getLogicalDevice();
            auto vertexAddress = meshPtr->getVertexBuffer().getDeviceAddress();
            auto indexAddress = meshPtr->getIndexBuffer().getDeviceAddress();
            auto primitiveCount = meshPtr->getIndexCount() / 3; // all triangles

            auto& geometry = geometries.emplace_back();
            geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
            geometry.geometry = vk::AccelerationStructureGeometryTrianglesDataKHR {
                    // vec3 triangle position, other attributes are passed via the vertex buffer used as a storage buffer (via descriptors) TODO: pass said attributes
                    .vertexFormat = vk::Format::eR32G32B32Sfloat,
                    .vertexData = vertexAddress,
                    .vertexStride = sizeof(Carrot::Vertex),
                    .maxVertex = static_cast<uint32_t>(meshPtr->getVertexCount()-1),
                    .indexType = vk::IndexType::eUint32,
                    .indexData = indexAddress,
                    .transformData = transformData->getDeviceAddress() + i * sizeof(vk::TransformMatrixKHR),
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
}

void Carrot::ASBuilder::createBuildCommandBuffers() {
    tlasBuildCommands = GetVulkanDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
            .commandPool = renderer.getVulkanDriver().getThreadComputeCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = GetEngine().getSwapchainImageCount(),
    });

    blasBuildCommands.resize(GetEngine().getSwapchainImageCount());
    compactBLASCommands.resize(GetEngine().getSwapchainImageCount());
}

void Carrot::ASBuilder::createQueryPools() {
    queryPools.resize(GetEngine().getSwapchainImageCount());
}

void Carrot::ASBuilder::createGraveyard() {
    asGraveyard.clear(); // we don't want to keep any old list
    asGraveyard.resize(GetEngine().getSwapchainImageCount());
}

void Carrot::ASBuilder::createDescriptors() {
    geometriesBuffer = nullptr;
    instancesBuffer = nullptr;
}

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes,
                                                                      const std::vector<glm::mat4>& transforms,
                                                                      const std::vector<std::uint32_t>& materials) {
    if(!enabled)
        return nullptr;
    Async::LockGuard l { access };
    return staticGeometries.create(meshes, transforms, materials, this);
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
    if(&renderContext.viewport != &GetEngine().getMainViewport()) {
        return;
    }
    ZoneScoped;
    Async::LockGuard l { access };
    auto purge = [](auto& pool) {
        pool.erase(std::find_if(WHOLE_CONTAINER(pool), [](auto a) { return a.second.expired(); }), pool.end());
    };
    purge(instances);
    purge(staticGeometries);

    asGraveyard[renderContext.swapchainIndex].clear();

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
        buildBottomLevels(renderContext, toBuild, false);
        for(auto& v : toBuild) {
            v->built = true;
        }
    }
    dirtyBlases = false;

    std::size_t activeInstances = 0;

    for(auto& [slot, valuePtr] : instances) {
        if(auto value = valuePtr.lock()) {
            if(value->enabled) {
                value->update();
                activeInstances++;
            }
        }
    }

    bool requireTLASRebuildNow = !toBuild.empty() || dirtyInstances || activeInstances > previousActiveInstances;

    dirtyInstances = false;
    if(requireTLASRebuildNow) {
        buildTopLevelAS(renderContext, false);
        framesBeforeRebuildingTLAS = 11;
    } else {
        if(framesBeforeRebuildingTLAS == 0 || !tlas) {
            buildTopLevelAS(renderContext, false);
            framesBeforeRebuildingTLAS = 11;
        } else {
            if(framesBeforeRebuildingTLAS > 0) {
                framesBeforeRebuildingTLAS--;
            }
            buildTopLevelAS(renderContext, true);
        }
    }

    if(geometriesBuffer)
        geometriesBufferPerFrame[renderContext.swapchainIndex] = geometriesBuffer->getWholeView();

    if(instancesBuffer)
        instancesBufferPerFrame[renderContext.swapchainIndex] = instancesBuffer->getWholeView();
}

void Carrot::ASBuilder::buildBottomLevels(const Carrot::Render::Context& renderContext, const std::vector<std::shared_ptr<BLASHandle>>& toBuild, bool dynamicGeometry) {
    ZoneScoped;
    if(!enabled)
        return;
    const vk::BuildAccelerationStructureFlagsKHR flags = dynamicGeometry ?
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate) :
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction);
    auto& device = renderer.getLogicalDevice();

    // add a bottom level AS for each geometry entry
    std::size_t blasCount = toBuild.size();

    auto& buildCommands = blasBuildCommands[renderContext.swapchainIndex];
    auto& compactCommands = compactBLASCommands[renderContext.swapchainIndex];
    vk::UniqueQueryPool& queryPool = queryPools[renderContext.swapchainIndex];
    if(buildCommands.size() < blasCount) {
        ZoneScopedN("Reallocate command buffers");
        buildCommands = GetVulkanDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo {
                .commandPool = renderer.getVulkanDriver().getThreadComputeCommandPool(),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = static_cast<std::uint32_t>(blasCount),
        });

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

    // allocate memory for each entry
    for(size_t index = 0; index < blasCount; index++) {
        ZoneScopedN("Create BLAS");
        ZoneValue(index);

        // TODO: can probably be cached
        auto& info = buildInfo[index];
        auto& blas = *toBuild[index];
        info.geometryCount = blas.geometries.size();
        info.flags = flags;
        info.pGeometries = blas.geometries.data();

        std::size_t geometryIndexStart = allGeometries.size();
        blas.firstGeometryIndex = geometryIndexStart;
        allGeometries.reserve(allGeometries.size() + blas.geometries.size());
        for (std::size_t j = 0; j < blas.geometries.size(); ++j) {
            allGeometries.emplace_back(SceneDescription::Geometry {
                .vertexBufferAddress = blas.geometries[j].geometry.triangles.vertexData.deviceAddress,
                .indexBufferAddress = blas.geometries[j].geometry.triangles.indexData.deviceAddress,
                .materialIndex = blas.materialSlots[j],
            });
        }

        info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        info.srcAccelerationStructure = nullptr;

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

        scratchSize = std::max(sizeInfo.buildScratchSize, scratchSize);

        originalSizes[index] = sizeInfo.accelerationStructureSize;
    }

    /*if(!geometriesBuffer || geometriesBuffer->getSize() < sizeof(SceneDescription::Geometry) * allGeometries.size())*/ {
        geometriesBuffer = GetResourceAllocator().allocateDedicatedBuffer(sizeof(SceneDescription::Geometry) * allGeometries.size(),
                                                                          vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                                          vk::MemoryPropertyFlagBits::eDeviceLocal);
        geometriesBuffer->setDebugNames("RT Geometries");
    }

    //geometriesBuffer->stageAsyncUploadWithOffset(*geometryUploadSemaphore[renderContext.swapchainIndex], 0, allGeometries.data(), allGeometries.size() * sizeof(SceneDescription::Geometry));
    geometriesBuffer->stageUploadWithOffset(0, allGeometries.data(), allGeometries.size() * sizeof(SceneDescription::Geometry));
    //GetEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eFragmentShader, *geometryUploadSemaphore[renderContext.swapchainIndex]);

    // TODO: reuse
    Buffer scratchBuffer = Buffer(renderer.getVulkanDriver(), scratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);
    scratchBuffer.name("BLAS build scratch buffer");

    bool compactAS = (flags & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) == vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;
    compactAS = false;

    auto scratchAddress = device.getBufferAddress({.buffer = scratchBuffer.getVulkanBuffer() });
    for(size_t index = 0; index < blasCount; index++) {
        ZoneScopedN("Record BLAS build command buffer");
        ZoneValue(index);

        auto& cmds = buildCommands[index];
        cmds->begin(vk::CommandBufferBeginInfo {
                .pInheritanceInfo = nullptr,
        });

        cmds->resetQueryPool(*queryPool, index, 1);

        buildInfo[index].scratchData.deviceAddress = scratchAddress;

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

        cmds->end();
    }

    {
        ZoneScopedN("Submit BLAS build commands");
        std::vector<vk::CommandBuffer> dereferencedBuildCommands{};
        dereferencedBuildCommands.reserve(buildCommands.size());
        for(auto& pBuffer : buildCommands) {
            dereferencedBuildCommands.push_back(*pBuffer);
        }
        GetVulkanDriver().submitCompute(vk::SubmitInfo {
                .commandBufferCount = static_cast<uint32_t>(toBuild.size()),
                .pCommandBuffers = dereferencedBuildCommands.data(),

                .signalSemaphoreCount = 1,
                .pSignalSemaphores = compactAS ? &(*preCompactBLASSemaphore[renderContext.swapchainIndex]) : &(*blasBuildSemaphore[renderContext.swapchainIndex]),
        });
        builtBLASThisFrame = true;
    }

#ifdef AFTERMATH_ENABLE
    try {
        renderer.getVulkanDriver().getGraphicsQueue().waitIdle();
    } catch (vk::DeviceLostError& e) {
        GetVulkanDriver().onDeviceLost();
    }
#endif

    if(false) {
        ZoneScopedN("Compress built Acceleration Structures");

        std::vector<vk::DeviceSize> compactSizes = device.getQueryPoolResults<vk::DeviceSize>(*queryPool, 0, blasCount, blasCount*sizeof(vk::DeviceSize), sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait);
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

        vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR;
        std::vector<vk::CommandBuffer> dereferencedCompactCommands{};
        dereferencedCompactCommands.reserve(compactCommands.size());
        for(auto& pBuffer : compactCommands) {
            dereferencedCompactCommands.push_back(*pBuffer);
        }
        GetVulkanDriver().submitCompute(vk::SubmitInfo {
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &(*preCompactBLASSemaphore[renderContext.swapchainIndex]),
            .pWaitDstStageMask = &waitStage,

            .commandBufferCount = static_cast<uint32_t>(toBuild.size()),
            .pCommandBuffers = dereferencedCompactCommands.data(),

            .signalSemaphoreCount = 1,
            .pSignalSemaphores = &(*blasBuildSemaphore[renderContext.swapchainIndex]),
        });
        // replace old AS with compacted AS
        for(size_t i = 0; i < blasCount; i++) {
            asGraveyard[renderContext.swapchainIndex].emplace_back(std::move(toBuild[i]->as));
            toBuild[i]->as = std::move(compactedASes[i]);
        }
    }
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
    ZoneScoped;

    auto& device = renderer.getLogicalDevice();
    const vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

    std::vector<vk::AccelerationStructureInstanceKHR> vkInstances{};
    std::vector<SceneDescription::Instance> logicalInstances { vkInstances.size() };
    vkInstances.reserve(instances.size());
    logicalInstances.reserve(instances.size());

    {
        ZoneScopedN("Convert to vulkan instances");
        for(const auto& [slot, v] : instances) {
            if(auto instance = v.lock()) {
                if(instance->enabled) {
                    if(auto pGeometry = instance->geometry.lock()) {
                        vkInstances.push_back(instance->instance);

                        logicalInstances.emplace_back(SceneDescription::Instance {
                                .firstGeometryIndex = pGeometry->firstGeometryIndex,
                        });
                    }
                }
            }
        }
        vkInstances.shrink_to_fit();
    }

    if(vkInstances.empty()) {
        tlas = nullptr;
        return;
    }

    vk::CommandBuffer& buildCommand = *tlasBuildCommands[renderContext.swapchainIndex];

    buildCommand.begin(vk::CommandBufferBeginInfo {
    });
    GetVulkanDriver().setFormattedMarker(buildCommand, "Start of command buffer for TLAS build, update = %d, framesBeforeRebuildingTLAS = %d", update, framesBeforeRebuildingTLAS);
    // upload instances to the device

    // FIXME: Culprit of gpu crash?
    /*if(!rtInstancesBuffer || lastInstanceCount < vkInstances.size()) */{
        rtInstancesBuffer = std::make_unique<Buffer>(renderer.getVulkanDriver(),
                                                   vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR),
                                                   vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                                                   vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
                                                   //vk::MemoryPropertyFlagBits::eDeviceLocal
        );

        rtInstancesBuffer->setDebugNames("TLAS Instances");
        lastInstanceCount = vkInstances.size();

        instanceBufferAddress = rtInstancesBuffer->getDeviceAddress();
    }

    /*if(!instancesBuffer || instancesBuffer->getSize() < sizeof(SceneDescription::Instance) * logicalInstances.size()) */{
        instancesBuffer = GetResourceAllocator().allocateDedicatedBuffer(sizeof(SceneDescription::Instance) * logicalInstances.size(),
                                                                                                        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                                                                                        vk::MemoryPropertyFlagBits::eDeviceLocal);
        instancesBuffer->setDebugNames("RT Instances");
    }

    {
        ZoneScopedN("Stage upload");
        //instancesBuffer->stageAsyncUploadWithOffset(*instanceUploadSemaphore[renderContext.swapchainIndex], 0, logicalInstances.data(), logicalInstances.size() * sizeof(SceneDescription::Instance));
        instancesBuffer->stageUploadWithOffset(0, logicalInstances.data(), logicalInstances.size() * sizeof(SceneDescription::Instance));
        //GetEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eFragmentShader, *instanceUploadSemaphore[renderContext.swapchainIndex]);

        //rtInstancesBuffer->stageUploadWithOffset(0, vkInstances.data(), vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
        rtInstancesBuffer->directUpload(vkInstances.data(), vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
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
            .srcAccelerationStructure = nullptr,
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
        tlas = std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo);
    }

    // Allocate scratch memory
    /*if(!scratchBuffer || lastScratchSize < sizeInfo.buildScratchSize) */{
        scratchBuffer = std::make_unique<Buffer>(renderer.getVulkanDriver(),
                                                 sizeInfo.buildScratchSize,
                                                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
                                                 vk::MemoryPropertyFlagBits::eDeviceLocal
        );
        scratchBuffer->name("RT instances build scratch buffer");
        lastScratchSize = sizeInfo.buildScratchSize;
        scratchBufferAddress = renderer.getLogicalDevice().getBufferAddress({
            .buffer = scratchBuffer->getVulkanBuffer(),
        });
    }

    buildInfo.srcAccelerationStructure = update ? tlas->getVulkanAS() : nullptr;
    buildInfo.dstAccelerationStructure = tlas->getVulkanAS();
    buildInfo.scratchData.deviceAddress = scratchBufferAddress;

    // one build offset per instance
    vk::AccelerationStructureBuildRangeInfoKHR buildOffsetInfo {
        .primitiveCount = static_cast<uint32_t>(vkInstances.size())
    };

    bottomLevelBarriers.push_back(vk::BufferMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eTransfer,
            .srcAccessMask = vk::AccessFlagBits2KHR::eTransferWrite,
            .dstStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
            .dstAccessMask = vk::AccessFlagBits2KHR::eMemoryRead,

            .srcQueueFamilyIndex = 0,
            .dstQueueFamilyIndex = 0,

            // TODO: will need to change with non-dedicated buffers
            .buffer = rtInstancesBuffer->getVulkanBuffer(),
            .offset = 0,
            .size = VK_WHOLE_SIZE,
    });
    vk::DependencyInfoKHR dependency {
            .bufferMemoryBarrierCount = static_cast<uint32_t>(bottomLevelBarriers.size()),
            .pBufferMemoryBarriers = bottomLevelBarriers.data(),
    };
    GetVulkanDriver().setFormattedMarker(buildCommand, "Pre-barrier TLAS build, update = %d, framesBeforeRebuildingTLAS = %d", update, framesBeforeRebuildingTLAS);
    buildCommand.pipelineBarrier2KHR(dependency);

    GetVulkanDriver().setFormattedMarker(buildCommand, "Before TLAS build, update = %d, framesBeforeRebuildingTLAS = %d, instanceCount = %llu, prevInstanceCount = %llu", update, framesBeforeRebuildingTLAS, vkInstances.size(), (std::size_t)prevPrimitiveCount);
    buildCommand.buildAccelerationStructuresKHR(buildInfo, &buildOffsetInfo);
    GetVulkanDriver().setFormattedMarker(buildCommand, "After TLAS build, update = %d, framesBeforeRebuildingTLAS = %d, instanceCount = %llu, prevInstanceCount = %llu", update, framesBeforeRebuildingTLAS, vkInstances.size(), (std::size_t)prevPrimitiveCount);
    buildCommand.end();

    std::vector<vk::Semaphore> waitSemaphores;
    std::vector<vk::PipelineStageFlags> waitStages;
    if(builtBLASThisFrame) {
        waitStages.emplace_back(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR);
        waitSemaphores.push_back(*blasBuildSemaphore[renderContext.swapchainIndex]);
    }

    GetVulkanDriver().submitCompute(vk::SubmitInfo {
        .waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
        .pWaitSemaphores = waitSemaphores.data(),
        .pWaitDstStageMask = waitStages.data(),

        .commandBufferCount = 1,
        .pCommandBuffers = &buildCommand,

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &(*tlasBuildSemaphore[renderContext.swapchainIndex]),
    });

    GetEngine().addWaitSemaphoreBeforeRendering(vk::PipelineStageFlagBits::eFragmentShader, *tlasBuildSemaphore[renderContext.swapchainIndex]);

    bottomLevelBarriers.clear();
    topLevelBarriers.push_back(vk::BufferMemoryBarrier2KHR {
            .srcStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
            .srcAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureWrite,
            .dstStageMask = vk::PipelineStageFlagBits2KHR::eAllGraphics,
            .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureRead,

            .srcQueueFamilyIndex = 0,
            .dstQueueFamilyIndex = 0,

            // TODO: will need to change with non-dedicated buffers
            .buffer = tlas->getBuffer().getVulkanBuffer(),
            .offset = 0,
            .size = VK_WHOLE_SIZE,
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

std::unique_ptr<Carrot::AccelerationStructure>& Carrot::ASBuilder::getTopLevelAS() {
    verify(GetCapabilities().supportsRaytracing, "Raytracing is not supported");
    return tlas;
}

void Carrot::ASBuilder::onSwapchainImageCountChange(size_t newCount) {
    createSemaphores();
    createBuildCommandBuffers();
    createQueryPools();
    createGraveyard();
    createDescriptors();

    instancesBufferPerFrame.clear();
    instancesBufferPerFrame.resize(newCount);

    geometriesBufferPerFrame.clear();
    geometriesBufferPerFrame.resize(newCount);
}

void Carrot::ASBuilder::onSwapchainSizeChange(int newWidth, int newHeight) {

}

Carrot::BufferView Carrot::ASBuilder::getGeometriesBuffer(const Render::Context& renderContext) {
    if(!geometriesBuffer) {
        return Carrot::BufferView{};
    }
    return geometriesBufferPerFrame[renderContext.swapchainIndex];
}

Carrot::BufferView Carrot::ASBuilder::getInstancesBuffer(const Render::Context& renderContext) {
    if(!instancesBuffer) {
        return Carrot::BufferView{};
    }
    return instancesBufferPerFrame[renderContext.swapchainIndex];
}
