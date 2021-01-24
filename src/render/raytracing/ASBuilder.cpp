//
// Created by jglrxavpok on 30/12/2020.
//

#include "ASBuilder.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <iostream>

Carrot::ASBuilder::ASBuilder(Carrot::Engine& engine): engine(engine) {

}

void Carrot::ASBuilder::buildBottomLevelAS() {
    const vk::BuildAccelerationStructureFlagBitsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    auto& device = engine.getLogicalDevice();

    // add a bottom level AS for each geometry entry
    size_t blasCount = bottomLevelGeometries.size();
    vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfo{blasCount};
    for(size_t index = 0; index < blasCount; index++) {
        auto& info = buildInfo[index];
        const auto& blas = bottomLevelGeometries[index];
        info.geometryCount = blas.geometries.size();
        info.flags = flags;
        info.pGeometries = blas.geometries.data();
        info.mode = vk::BuildAccelerationStructureModeKHR::eBuild; // TODO: change for updates
        info.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
        info.srcAccelerationStructure = nullptr; // TODO: change for updates
    }

    vk::DeviceSize scratchSize = 0;
    vector<vk::DeviceSize> originalSizes(bottomLevelGeometries.size());
    // allocate memory for each entry
    for(size_t index = 0; index < blasCount; index++) {
        vector<uint32_t> primitiveCounts(bottomLevelGeometries[index].buildRanges.size());
        // copy primitive counts to flat vector
        for(size_t geomIndex = 0; geomIndex < primitiveCounts.size(); geomIndex++) {
            primitiveCounts[geomIndex] = bottomLevelGeometries[index].buildRanges[geomIndex].primitiveCount;
        }

        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo =
                device.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo[index], primitiveCounts);

        vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = sizeInfo.accelerationStructureSize,
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
        };

        bottomLevelGeometries[index].as = move(make_unique<AccelerationStructure>(engine, createInfo));
        buildInfo[index].dstAccelerationStructure = bottomLevelGeometries[index].as->getVulkanAS();

        scratchSize = std::max(sizeInfo.buildScratchSize, scratchSize);

        originalSizes[index] = sizeInfo.accelerationStructureSize;
    }

    unique_ptr<Buffer> scratchBuffer = make_unique<Buffer>(engine, scratchSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);

    bool compactAS = (flags & vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction) == vk::BuildAccelerationStructureFlagBitsKHR::eAllowCompaction;

    vk::QueryPoolCreateInfo queryPoolCreateInfo {
        .queryType = vk::QueryType::eAccelerationStructureCompactedSizeKHR,
        .queryCount = static_cast<uint32_t>(bottomLevelGeometries.size()),
    };

    vk::UniqueQueryPool queryPool = device.createQueryPoolUnique(queryPoolCreateInfo, engine.getAllocator());

    vector<vk::CommandBuffer> buildCommands = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = engine.getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = static_cast<uint32_t>(bottomLevelGeometries.size()),
    });

    auto scratchAddress = device.getBufferAddress({.buffer = scratchBuffer->getVulkanBuffer() });
    for(size_t index = 0; index < bottomLevelGeometries.size(); index++) {
        auto& cmds = buildCommands[index];
        cmds.begin(vk::CommandBufferBeginInfo {
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                .pInheritanceInfo = nullptr,
        });
        buildInfo[index].scratchData.deviceAddress = scratchAddress;

        vector<const vk::AccelerationStructureBuildRangeInfoKHR*> pBuildRanges{bottomLevelGeometries[index].buildRanges.size()};
        for(size_t i = 0; i < bottomLevelGeometries[index].buildRanges.size(); i++) {
            pBuildRanges[i] = &bottomLevelGeometries[index].buildRanges[i];
        }

#ifdef AFTERMATH_ENABLE
        cmds.setCheckpointNV("Before AS build");
#endif
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
            cmds.writeAccelerationStructuresPropertiesKHR(bottomLevelGeometries[index].as->getVulkanAS(), vk::QueryType::eAccelerationStructureCompactedSizeKHR, *queryPool, index);
        }

        cmds.end();
    }

    engine.getGraphicsQueue().submit(vk::SubmitInfo {
        .commandBufferCount = static_cast<uint32_t>(buildCommands.size()),
        .pCommandBuffers = buildCommands.data(),
    });
#ifdef AFTERMATH_ENABLE
    try {
        engine.getGraphicsQueue().waitIdle();
    } catch (std::exception& e) {
        std::this_thread::sleep_for(std::chrono::seconds(6));
        exit(1);
    }
#else
    engine.getGraphicsQueue().waitIdle();
#endif

    if(compactAS) {
        vector<vk::DeviceSize> compactSizes = device.getQueryPoolResults<vk::DeviceSize>(*queryPool, 0, bottomLevelGeometries.size(), bottomLevelGeometries.size()*sizeof(vk::DeviceSize), sizeof(vk::DeviceSize), vk::QueryResultFlagBits::eWait);
        vector<unique_ptr<AccelerationStructure>> compactedAS{};
        for (size_t index = 0; index < bottomLevelGeometries.size(); index++) {
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

            auto compactAS = make_unique<AccelerationStructure>(engine, createInfo);

            // copy AS
            cmds.copyAccelerationStructureKHR(vk::CopyAccelerationStructureInfoKHR {
                .src = bottomLevelGeometries[index].as->getVulkanAS(),
                .dst = compactAS->getVulkanAS(),
                .mode = vk::CopyAccelerationStructureModeKHR::eCompact
            });

            compactedAS.emplace_back(move(compactAS));
            cmds.end();
        }
        engine.getGraphicsQueue().submit(vk::SubmitInfo {
            .commandBufferCount = static_cast<uint32_t>(buildCommands.size()),
            .pCommandBuffers = buildCommands.data(),
        });
        engine.getGraphicsQueue().waitIdle();
        // replace old AS with compacted AS
        for(size_t i = 0; i < bottomLevelGeometries.size(); i++) {
            bottomLevelGeometries[i].as = move(compactedAS[i]);
        }
    }

    device.freeCommandBuffers(engine.getGraphicsCommandPool(), buildCommands);
}

void Carrot::ASBuilder::addInstance(const InstanceInput instance) {
    topLevelInstances.push_back(instance);
}

vk::AccelerationStructureInstanceKHR Carrot::ASBuilder::convertToVulkanInstance(const InstanceInput& instance) {
    vk::AccelerationStructureInstanceKHR vkInstance{};
    auto transposedTransform = glm::transpose(instance.transform);
    // extract 4x3 matrix
    std::memcpy(&vkInstance.transform, glm::value_ptr(transposedTransform), sizeof(vkInstance.transform));

    vkInstance.instanceCustomIndex = instance.customInstanceIndex;

    // TODO: custom flags
    vkInstance.setFlags(vk::GeometryInstanceFlagBitsKHR::eForceOpaque | vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable);

    vkInstance.instanceShaderBindingTableRecordOffset = instance.hitGroup;
    vkInstance.mask = instance.mask;

    assert(instance.geometryIndex < bottomLevelGeometries.size());

    auto& blas = bottomLevelGeometries[instance.geometryIndex];

    vk::AccelerationStructureDeviceAddressInfoKHR addressInfo {
        .accelerationStructure = blas.as->getVulkanAS()
    };
    auto blasAddress = engine.getLogicalDevice().getAccelerationStructureAddressKHR(addressInfo);

    vkInstance.accelerationStructureReference = blasAddress;

    return vkInstance;
}

void Carrot::ASBuilder::buildTopLevelAS(bool update) {
    auto& device = engine.getLogicalDevice();
    const vk::BuildAccelerationStructureFlagsKHR flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
    vk::CommandBuffer buildCommand = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = engine.getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    })[0];

    vector<vk::AccelerationStructureInstanceKHR> instances{};
    instances.reserve(topLevelInstances.size());

    for(size_t i = 0; i < topLevelInstances.size(); i++) {
        instances.push_back(convertToVulkanInstance(topLevelInstances[i]));
    }

    buildCommand.begin(vk::CommandBufferBeginInfo {
        .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
    });
    // upload instances to the device
    instancesBuffer = make_unique<Buffer>(engine,
                                          instances.size() * sizeof(vk::AccelerationStructureInstanceKHR),
                                          vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                          vk::MemoryPropertyFlagBits::eDeviceLocal
                                          );

    instancesBuffer->setDebugNames("TLAS Instances");
    instancesBuffer->stageUploadWithOffsets(make_pair(0ull, instances));

    vk::BufferDeviceAddressInfo bufferInfo {
        .buffer = instancesBuffer->getVulkanBuffer()
    };
    auto instanceAddress = engine.getLogicalDevice().getBufferAddress(bufferInfo);

    vk::AccelerationStructureGeometryInstancesDataKHR instancesVk {
        .arrayOfPointers = false,
    };
    instancesVk.data.deviceAddress = instanceAddress;

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
    vk::AccelerationStructureBuildSizesInfoKHR sizeInfo = engine.getLogicalDevice().getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, buildInfo, count);

    if( ! update) {
        vk::AccelerationStructureCreateInfoKHR createInfo {
                .size = sizeInfo.accelerationStructureSize,
                .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        };
        tlas.as = make_unique<AccelerationStructure>(engine, createInfo);
    }

    // Allocate scratch memory
    auto scratchBuffer = make_unique<Buffer>(engine,
                                             sizeInfo.buildScratchSize,
                                             vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                             vk::MemoryPropertyFlagBits::eDeviceLocal
                                             );

    bufferInfo.buffer = scratchBuffer->getVulkanBuffer();
    auto scratchAddress = engine.getLogicalDevice().getBufferAddress(bufferInfo);

    buildInfo.srcAccelerationStructure = update ? tlas.as->getVulkanAS() : nullptr;
    buildInfo.dstAccelerationStructure = tlas.as->getVulkanAS();
    buildInfo.scratchData.deviceAddress = scratchAddress;

    // one build offset per instance
    vk::AccelerationStructureBuildRangeInfoKHR buildOffsetInfo {
        .primitiveCount = static_cast<uint32_t>(instances.size())
    };
    buildCommand.buildAccelerationStructuresKHR(buildInfo, &buildOffsetInfo);

    buildCommand.end();

    engine.getGraphicsQueue().submit(vk::SubmitInfo {
       .commandBufferCount = 1,
       .pCommandBuffers = &buildCommand,
    });
    engine.getGraphicsQueue().waitIdle();

    device.freeCommandBuffers(engine.getGraphicsCommandPool(), buildCommand);
}