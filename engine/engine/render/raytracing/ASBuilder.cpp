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
    BLASHandle::BLASHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, const Carrot::Mesh& mesh):
    WeakPoolHandle(index, std::move(destructor)),
    mesh(mesh) {
        TODO // prepare geometries
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

std::shared_ptr<Carrot::BLASHandle> Carrot::ASBuilder::addStaticMesh(const Carrot::Mesh& mesh) {
    if(!enabled)
        return nullptr;
    return staticGeometries.create(mesh);
}

void Carrot::ASBuilder::onFrame(const Carrot::Render::Context& renderContext) {
    if(!enabled)
        return;
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

    buildBottomLevels(toBuild, false);
    for(auto& v : toBuild) {
        v->built = true;
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
        buildInfo[index].scratchData.deviceAddress = scratchAddress;

        std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRanges{toBuild[index]->buildRanges.size()};
        for(size_t i = 0; i < toBuild[index]->buildRanges.size(); i++) {
            pBuildRanges[i] = &toBuild[index]->buildRanges[i];
        }

#ifdef AFTERMATH_ENABLE
        cmds.setCheckpointNV("Before AS build");
#endif
        assert(pBuildRanges.size() == 1);
        // build AS
        cmds.buildAccelerationStructuresKHR(buildInfo[index], pBuildRanges);
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

    renderer.getVulkanDriver().getGraphicsQueue().submit(vk::SubmitInfo {
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
        renderer.getVulkanDriver().getGraphicsQueue().submit(vk::SubmitInfo {
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
                vkInstances.push_back(instance->instance);
            }
        }
    }

    buildCommand.begin(vk::CommandBufferBeginInfo {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    // upload instances to the device
    if(!instancesBuffer || lastInstanceCount < vkInstances.size()) {
        instancesBuffer = std::make_unique<Buffer>(renderer.getVulkanDriver(),
                                                   vkInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR),
                                                   vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
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
    size_t count = instances.size();
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
                                                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
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
        .primitiveCount = static_cast<uint32_t>(instances.size())
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

    renderer.getVulkanDriver().getGraphicsQueue().submit(vk::SubmitInfo {
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

/* TODO
Carrot::TLAS& Carrot::ASBuilder::getTopLevelAS() {
    assert(enabled);
    return tlas;
}*/

/*
void Carrot::ASBuilder::updateBottomLevelAS(const std::vector<size_t>& blasIndices, vk::Semaphore skinningSemaphore) {
    if(!enabled)
        return;
    ZoneScoped;
    auto& device = renderer.getLogicalDevice();
    const vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastBuild | vk::BuildAccelerationStructureFlagBitsKHR::eAllowUpdate;

    renderer.getVulkanDriver().performSingleTimeGraphicsCommands([&](vk::CommandBuffer& commands) {
        PrepareVulkanTracy2(renderer.getEngine(), commands);

        TracyVulkanZone2(renderer.getEngine(), commands, "Update BLASes");

        std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfoList{blasIndices.size()};
        std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> buildRangeList{blasIndices.size()};
        for (int i = 0; i < blasIndices.size(); ++i) {
            auto blasIndex = blasIndices[i];
            auto& blas = bottomLevelGeometries[blasIndex];

            if(!blas.cachedBuildInfo) {
                blas.cachedBuildInfo = std::make_unique<vk::AccelerationStructureBuildGeometryInfoKHR>();

                auto& buildInfo = *blas.cachedBuildInfo;

                buildInfo = {
                        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
                        .flags = flags,
                        .mode = vk::BuildAccelerationStructureModeKHR::eUpdate,
                        .srcAccelerationStructure = blas.as->getVulkanAS(),
                        .dstAccelerationStructure = blas.as->getVulkanAS(),
                        .geometryCount = static_cast<uint32_t>(blas.geometries.size()),
                        .pGeometries = blas.geometries.data(),
                };

                // figure scratch size required to build
                std::vector<uint32_t> primitiveCounts(blas.buildRanges.size());
                // copy primitive counts to flat vector
                for(size_t geomIndex = 0; geomIndex < primitiveCounts.size(); geomIndex++) {
                    primitiveCounts[geomIndex] = blas.buildRanges[geomIndex].primitiveCount;
                }

                vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
                        device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, primitiveCounts);

                vk::AccelerationStructureCreateInfoKHR createInfo {
                        .size = sizeInfo.accelerationStructureSize,
                        .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
                };

                auto scratchSize = sizeInfo.buildScratchSize;
                blas.scratchBuffer = std::move(std::make_unique<Buffer>(renderer.getVulkanDriver(), scratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal));
                buildInfo.scratchData.deviceAddress = blas.scratchBuffer->getDeviceAddress();

                std::vector<const vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRanges(blas.buildRanges.size());
                for(size_t i = 0; i < blas.buildRanges.size(); i++) {
                    pBuildRanges[i] = &blas.buildRanges[i];
                }

                blas.cachedBuildRanges = pBuildRanges;
            }

           // commands.buildAccelerationStructuresKHR(*blas.cachedBuildInfo, blas.cachedBuildRanges);
            buildInfoList[i] = *blas.cachedBuildInfo;
            assert(blas.cachedBuildRanges.size() == 1);
            buildRangeList[i] = blas.cachedBuildRanges[0];

            bottomLevelBarriers.push_back(vk::BufferMemoryBarrier2KHR {
                    .srcStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                    .srcAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureWrite,
                    .dstStageMask = vk::PipelineStageFlagBits2KHR::eAccelerationStructureBuild,
                    .dstAccessMask = vk::AccessFlagBits2KHR::eAccelerationStructureRead,

                    .srcQueueFamilyIndex = 0,
                    .dstQueueFamilyIndex = 0,

                    // TODO: will need change with non-dedicated buffers
                    .buffer = blas.as->getBuffer().getVulkanBuffer(),
                    .offset = 0,
                    .size = VK_WHOLE_SIZE,
            });
        }
        commands.buildAccelerationStructuresKHR(buildInfoList, buildRangeList);
    }, false /* don't wait for this command to end, we use pipeline barriers * /, skinningSemaphore, vk::PipelineStageFlagBits::eComputeShader);
}
*/
void Carrot::ASBuilder::updateTopLevelAS() {
    if(!enabled)
        return;
    buildTopLevelAS(true, false);
}