//
// Created by jglrxavpok on 30/12/2020.
//

#include "AccelerationStructure.h"
#include "engine/render/resources/Buffer.h"

Carrot::AccelerationStructure::AccelerationStructure(Carrot::VulkanDriver& driver,
                                                     vk::AccelerationStructureCreateInfoKHR& createInfo): driver(driver) {
    // allocate buffer to store AS
    buffer = make_unique<Buffer>(driver,
                                 createInfo.size,
                                 vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                 vk::MemoryPropertyFlagBits::eDeviceLocal);

    createInfo.buffer = buffer->getVulkanBuffer();

    as = driver.getLogicalDevice().createAccelerationStructureKHRUnique(createInfo, driver.getAllocationCallbacks());
}

vk::AccelerationStructureKHR& Carrot::AccelerationStructure::getVulkanAS() {
    return *as;
}
