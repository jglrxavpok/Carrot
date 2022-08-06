//
// Created by jglrxavpok on 30/12/2020.
//

#include "ASBuilder.h"
#include <engine/vulkan/includes.h>
#include <engine/render/resources/Mesh.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>
#include "RayTracer.h"

namespace Carrot {
    BLASHandle::BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes):
    WeakPoolHandle(index, std::move(destructor)),
    meshes(meshes) {
        geometries.reserve(meshes.size());
        buildRanges.reserve(meshes.size());

        for(auto& meshPtr : meshes) {
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
                    .maxVertex = static_cast<uint32_t>(meshPtr->getVertexCount()),
                    .indexType = vk::IndexType::eUint32,
                    .indexData = indexAddress,
                    .transformData = {},
            };

            buildRanges.emplace_back(vk::AccelerationStructureBuildRangeInfoKHR {
                    .primitiveCount = static_cast<uint32_t>(primitiveCount),
                    .primitiveOffset = 0,
                    .firstVertex = 0,
                    .transformOffset = 0,
            });
        }
    }

    void BLASHandle::update() {
        // no op for now
    }

    void BLASHandle::setDirty() {
        built = false;
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
        assert(geometryPtr);

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

    createSemaphores();
    createBuildCommandBuffers();
    createQueryPools();
    createGraveyard();
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

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes) {
    if(!enabled)
        return nullptr;
    Async::LockGuard l { access };
    return staticGeometries.create(meshes);
}

void Carrot::ASBuilder::createSemaphores() {
    std::size_t imageCount = GetEngine().getSwapchainImageCount();
    preCompactBLASSemaphore.resize(imageCount);
    blasBuildSemaphore.resize(imageCount);
    tlasBuildSemaphore.resize(imageCount);
    for (size_t i = 0; i < imageCount; ++i) {
        preCompactBLASSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        blasBuildSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
        tlasBuildSemaphore[i] = GetVulkanDevice().createSemaphoreUnique({});
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
            if(!value->isBuilt()) {
                toBuild.push_back(value);
            }
        }
    }

    builtBLASThisFrame = false;
    if(!toBuild.empty()) {
        buildBottomLevels(renderContext, toBuild, false);
        for(auto& v : toBuild) {
            v->built = true;
        }
    }

    for(auto& [slot, valuePtr] : instances) {
        if(auto value = valuePtr.lock()) {
            if(value->enabled) {
                value->update();
            }
        }
    }

    if(!toBuild.empty()) {
        buildTopLevelAS(renderContext, false);
        framesBeforeRebuildingTLAS = 10;
    } else {
        if(framesBeforeRebuildingTLAS == 0 || !tlas) {
            buildTopLevelAS(renderContext, false);
            framesBeforeRebuildingTLAS = 10;
        } else {
            if(framesBeforeRebuildingTLAS > 0) {
                framesBeforeRebuildingTLAS--;
            }
            buildTopLevelAS(renderContext, true);
        }
    }
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

    // TODO: reuse
    Buffer scratchBuffer = Buffer(renderer.getVulkanDriver(), scratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

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

#ifdef AFTERMATH_ENABLE
        cmds->setCheckpointNV("Before AS build");
#endif
        // build AS
        cmds->buildAccelerationStructuresKHR(buildInfo[index], pBuildRanges.data());
#ifdef AFTERMATH_ENABLE
        cmds->setCheckpointNV("After AS build");
#endif
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
    } catch (std::exception& e) {
        std::this_thread::sleep_for(std::chrono::seconds(6));
        exit(1);
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
    return instances.create(correspondingGeometry);
}

void Carrot::ASBuilder::buildTopLevelAS(const Carrot::Render::Context& renderContext, bool update) {
    if(!enabled)
        return;
    ZoneScoped;

    if(!update && tlas) {
        ZoneScopedN("WaitDeviceIdle");
        //GetVulkanDriver().getComputeQueue().waitIdle();
        //WaitDeviceIdle();
    }

    auto& device = renderer.getLogicalDevice();
    const vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

    std::vector<vk::AccelerationStructureInstanceKHR> vkInstances{};
    vkInstances.reserve(instances.size());

    {
        ZoneScopedN("Convert to vulkan instances");
        for(const auto& [slot, v] : instances) {
            if(auto instance = v.lock()) {
                if(instance->enabled) {
                    vkInstances.push_back(instance->instance);
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
    // upload instances to the device
    if(!instancesBuffer || lastInstanceCount < vkInstances.size()) {
        instancesBuffer = std::make_unique<Buffer>(renderer.getVulkanDriver(),
                                                   vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR),
                                                   vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
                                                   vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );

        instancesBuffer->setDebugNames("TLAS Instances");
        lastInstanceCount = vkInstances.size();

        instanceBufferAddress = renderer.getLogicalDevice().getBufferAddress({
            .buffer = instancesBuffer->getVulkanBuffer(),
        });
    }
    {
        ZoneScopedN("Stage upload");
        instancesBuffer->directUpload(vkInstances.data(), vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR));
        // TODO: remove wait
        //instancesBuffer->stageUploadWithOffsets(make_pair(0ull, instances));
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

    if( ! update) {
        vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = sizeInfo.accelerationStructureSize,
                .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        };
        tlas = std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo);
    }

    // Allocate scratch memory
    if(!scratchBuffer || lastScratchSize < sizeInfo.buildScratchSize) {
        scratchBuffer = std::make_unique<Buffer>(renderer.getVulkanDriver(),
                                                 sizeInfo.buildScratchSize,
                                                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eStorageBuffer,
                                                 vk::MemoryPropertyFlagBits::eDeviceLocal
        );
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
            .buffer = instancesBuffer->getVulkanBuffer(),
            .offset = 0,
            .size = VK_WHOLE_SIZE,
    });
    vk::DependencyInfoKHR dependency {
            .bufferMemoryBarrierCount = static_cast<uint32_t>(bottomLevelBarriers.size()),
            .pBufferMemoryBarriers = bottomLevelBarriers.data(),
    };
    buildCommand.pipelineBarrier2KHR(dependency);

    buildCommand.buildAccelerationStructuresKHR(buildInfo, &buildOffsetInfo);

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
}

void Carrot::ASBuilder::onSwapchainSizeChange(int newWidth, int newHeight) {

}
