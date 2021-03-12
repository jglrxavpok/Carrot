//
// Created by jglrxavpok on 30/12/2020.
//

#include "AccelerationStructure.h"
#include "engine/render/resources/Buffer.h"

Carrot::AccelerationStructure::AccelerationStructure(Carrot::Engine& engine,
                                                     vk::AccelerationStructureCreateInfoKHR& createInfo): engine(engine) {
    // allocate buffer to store AS
    buffer = make_unique<Buffer>(engine.getVulkanDevice(),
                                 createInfo.size,
                                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                 vk::MemoryPropertyFlagBits::eDeviceLocal);

    createInfo.buffer = buffer->getVulkanBuffer();

    as = engine.getLogicalDevice().createAccelerationStructureKHRUnique(createInfo, engine.getAllocator());
}

vk::AccelerationStructureKHR& Carrot::AccelerationStructure::getVulkanAS() {
    return *as;
}
