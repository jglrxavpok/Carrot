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
            auto vertexIndexAddress = device.getBufferAddress({ .buffer = meshPtr->getBackingBuffer().getVulkanBuffer() });
            auto primitiveCount = meshPtr->getIndexCount() / 3; // all triangles

            auto& geometry = geometries.emplace_back();
            geometry.geometryType = vk::GeometryTypeKHR::eTriangles;
            geometry.geometry = vk::AccelerationStructureGeometryTrianglesDataKHR {
                    // vec3 triangle position, other attributes are passed via the vertex buffer used as a storage buffer (via descriptors) TODO: pass said attributes
                    .vertexFormat = vk::Format::eR32G32B32Sfloat,
                    .vertexData = vertexIndexAddress + meshPtr->getVertexStartOffset(),
                    .vertexStride = sizeof(Carrot::Vertex),
                    .maxVertex = static_cast<uint32_t>(meshPtr->getVertexCount()),
                    .indexType = vk::IndexType::eUint32,
                    .indexData = vertexIndexAddress + meshPtr->getIndexStartOffset(),
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
}

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addBottomLevel(const std::vector<std::shared_ptr<Carrot::Mesh>>& meshes) {
    if(!enabled)
        return nullptr;
    return staticGeometries.create(meshes);
}

void Carrot::ASBuilder::onFrame(const Carrot::Render::Context& renderContext) {
    if(!enabled)
        return;
    if(&renderContext.viewport != &GetEngine().getMainViewport()) {
        return;
    }
    auto purge = [](auto& pool) {
        pool.erase(std::find_if(WHOLE_CONTAINER(pool), [](auto a) { return a.second.expired(); }), pool.end());
    };
    purge(instances);
    purge(staticGeometries);

    std::vector<std::shared_ptr<BLASHandle>> toBuild;
    for(auto& [slot, valuePtr] : staticGeometries) {
        if(auto value = valuePtr.lock()) {
            if(!value->isBuilt()) {
                toBuild.push_back(value);
            }
        }
    }

    if(!toBuild.empty()) {
        buildBottomLevels(toBuild, false);
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
        buildTopLevelAS(false, true);
        framesBeforeRebuildingTLAS = 10;
    } else {
        if(framesBeforeRebuildingTLAS == 0 || !tlas) {
            buildTopLevelAS(false, true);
            framesBeforeRebuildingTLAS = 10;
        } else {
            if(framesBeforeRebuildingTLAS > 0) {
                framesBeforeRebuildingTLAS--;
            }
            buildTopLevelAS(true, true);
        }
    }
}

void Carrot::ASBuilder::buildBottomLevels(const std::vector<std::shared_ptr<BLASHandle>>& toBuild, bool dynamicGeometry) {
    if(!enabled)
        return;
    const vk::BuildAccelerationStructureFlagsKHR flags = dynamicGeometry ?
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate) :
            (vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace | vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction);
    auto& device = renderer.getLogicalDevice();

    // add a bottom level AS for each geometry entry
    std::size_t blasCount = toBuild.size();
    std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfo{blasCount};
    for(size_t index = 0; index < blasCount; index++) {
        auto& info = buildInfo[index];
        const auto& blas = *toBuild[index];
        info.geometryCount = blas.geometries.size();
        info.flags = flags;
        info.pGeometries = blas.geometries.data();
        info.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
        info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        info.srcAccelerationStructure = nullptr;
    }

    vk::DeviceSize scratchSize = 0;
    std::vector<vk::DeviceSize> originalSizes(blasCount);
    // allocate memory for each entry
    for(size_t index = 0; index < blasCount; index++) {
        std::vector<uint32_t> primitiveCounts(toBuild[index]->buildRanges.size());
        // copy primitive counts to flat vector
        for(size_t geomIndex = 0; geomIndex < primitiveCounts.size(); geomIndex++) {
            primitiveCounts[geomIndex] = toBuild[index]->buildRanges[geomIndex].primitiveCount;
        }

        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
                device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo[index], primitiveCounts);

        vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = sizeInfo.accelerationStructureSize,
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        };

        toBuild[index]->as = std::move(std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo));
        buildInfo[index].dstAccelerationStructure = toBuild[index]->as->getVulkanAS();

        scratchSize = std::max(sizeInfo.buildScratchSize, scratchSize);

        originalSizes[index] = sizeInfo.accelerationStructureSize;
    }

    Buffer scratchBuffer = Buffer(renderer.getVulkanDriver(), scratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

    bool compactAS = (flags & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) == vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;

    vk::QueryPoolCreateInfo queryPoolCreateInfo {
        .queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
        .queryCount = static_cast<uint32_t>(blasCount),
    };

    vk::UniqueQueryPool queryPool = device.createQueryPoolUnique(queryPoolCreateInfo, renderer.getVulkanDriver().getAllocationCallbacks());

    std::vector<vk::CommandBuffer> buildCommands = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = renderer.getVulkanDriver().getThreadGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(blasCount),
    });

    auto scratchAddress = device.getBufferAddress({.buffer = scratchBuffer.getVulkanBuffer() });
    for(size_t index = 0; index < blasCount; index++) {
        auto& cmds = buildCommands[index];
        cmds.begin(vk::CommandBufferBeginInfo {
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                .pInheritanceInfo = nullptr,
        });

        cmds.resetQueryPool(*queryPool, index, 1);

        buildInfo[index].scratchData.deviceAddress = scratchAddress;

        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> pBuildRanges{toBuild[index]->buildRanges.size()};
        for(size_t i = 0; i < toBuild[index]->buildRanges.size(); i++) {
            pBuildRanges[i] = toBuild[index]->buildRanges[i];
        }

#ifdef AFTERMATH_ENABLE
        cmds.setCheckpointNV("Before AS build");
#endif
        // build AS
        cmds.buildAccelerationStructuresKHR(buildInfo[index], pBuildRanges.data());
#ifdef AFTERMATH_ENABLE
        cmds.setCheckpointNV("After AS build");
#endif
        vk::MemoryBarrier barrier {
            .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
            .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR,
        };
        cmds.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, static_cast<vk::DependencyFlags>(0), barrier, {}, {});

        // write compacted size
        if(compactAS) {
            cmds.writeAccelerationStructuresPropertiesKHR(toBuild[index]->as->getVulkanAS(), vk::QueryType::eAccelerationStructureCompactedSizeKHR, *queryPool, index);
        }

        cmds.end();
    }

    GetVulkanDriver().submitGraphics(vk::SubmitInfo {
        .commandBufferCount = static_cast<uint32_t>(buildCommands.size()),
        .pCommandBuffers = buildCommands.data(),
    });
#ifdef AFTERMATH_ENABLE
    try {
        renderer.getVulkanDriver().getGraphicsQueue().waitIdle();
    } catch (std::exception& e) {
        std::this_thread::sleep_for(std::chrono::seconds(6));
        exit(1);
    }
#else
    renderer.getVulkanDriver().getGraphicsQueue().waitIdle();
#endif

    if(compactAS) {
        std::vector<vk::DeviceSize> compactSizes = device.getQueryPoolResults<vk::DeviceSize>(*queryPool, 0, blasCount, blasCount*sizeof(vk::DeviceSize), sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait);
        std::vector<std::unique_ptr<AccelerationStructure>> compactedAS{};
        for (size_t index = 0; index < blasCount; index++) {
            auto& cmds = buildCommands[index];
            cmds.begin(vk::CommandBufferBeginInfo{
                    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                    .pInheritanceInfo = nullptr,
            });

            // create new AS
            vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = compactSizes[index],
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
            };

            auto compactAS = std::make_unique<AccelerationStructure>(renderer.getVulkanDriver(), createInfo);

            // copy AS
            cmds.copyAccelerationStructureKHR(vk::CopyAccelerationStructureInfoKHR {
                .src = toBuild[index]->as->getVulkanAS(),
                .dst = compactAS->getVulkanAS(),
                .mode = vk::CopyAccelerationStructureModeKHR::eCompact
            });

            compactedAS.emplace_back(move(compactAS));
            cmds.end();
        }
        GetVulkanDriver().submitGraphics(vk::SubmitInfo {
            .commandBufferCount = static_cast<uint32_t>(buildCommands.size()),
            .pCommandBuffers = buildCommands.data(),
        });
        renderer.getVulkanDriver().getGraphicsQueue().waitIdle();
        // replace old AS with compacted AS
        for(size_t i = 0; i < blasCount; i++) {
            toBuild[i]->as = move(compactedAS[i]);
        }
    }

    device.freeCommandBuffers(renderer.getVulkanDriver().getThreadGraphicsCommandPool(), buildCommands);
}

std::shared_ptr<Carrot::InstanceHandle> Carrot::ASBuilder::addInstance(std::weak_ptr<Carrot::BLASHandle> correspondingGeometry) {
    if(!enabled)
        return nullptr;
    return instances.create(correspondingGeometry);
}

void Carrot::ASBuilder::buildTopLevelAS(bool update, bool waitForCompletion) {
    if(!enabled)
        return;
    ZoneScoped;

    if(!update && tlas) {
        ZoneScopedN("WaitDeviceIdle");
        WaitDeviceIdle();
    }

    auto& device = renderer.getLogicalDevice();
    const vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;
    if(!tlasBuildCommands) {
        tlasBuildCommands = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
                .commandPool = renderer.getVulkanDriver().getThreadGraphicsCommandPool(),
                .level = vk::CommandBufferLevel::ePrimary,
                .commandBufferCount = 1,
        })[0];
    }
    vk::CommandBuffer& buildCommand = tlasBuildCommands;
    buildCommand.reset();

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

    buildCommand.begin(vk::CommandBufferBeginInfo {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
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

    GetVulkanDriver().submitGraphics(vk::SubmitInfo {
       .commandBufferCount = 1,
       .pCommandBuffers = &buildCommand,
    });

    bottomLevelBarriers.clear();
    if(waitForCompletion) {
        renderer.getVulkanDriver().getGraphicsQueue().waitIdle();
    } else {
        topLevelBarriers.push_back(vk::BufferMemoryBarrier2KHR {
                .srcStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                .srcAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureWrite,
                .dstStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureRead,

                .srcQueueFamilyIndex = 0,
                .dstQueueFamilyIndex = 0,

                // TODO: will need to change with non-dedicated buffers
                .buffer = tlas->getBuffer().getVulkanBuffer(),
                .offset = 0,
                .size = VK_WHOLE_SIZE,
        });
    }
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

void Carrot::ASBuilder::updateTopLevelAS() {
    if(!enabled)
        return;
    buildTopLevelAS(true, false);
}

std::unique_ptr<Carrot::AccelerationStructure>& Carrot::ASBuilder::getTopLevelAS() {
    verify(GetCapabilities().supportsRaytracing, "Raytracing is not supported");
    return tlas;
}
