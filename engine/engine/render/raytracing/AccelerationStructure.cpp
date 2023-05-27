//
// Created by jglrxavpok on 30/12/2020.
//

#include "AccelerationStructure.h"
#include "engine/render/resources/Buffer.h"

Carrot::Async::ParallelMap<vk::DeviceAddress, const Carrot::AccelerationStructure*> Carrot::AccelerationStructure::ASByStartAddress;

Carrot::AccelerationStructure::AccelerationStructure(Carrot::VulkanDriver& driver,
                                                     vk::AccelerationStructureCreateInfoKHR& createInfo) {
    // allocate buffer to store AS
    buffer = std::make_unique<Buffer>(GetVulkanDriver(),
                                      createInfo.size,
                                      vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                      vk::MemoryPropertyFlagBits::eDeviceLocal);

    createInfo.buffer = buffer->getVulkanBuffer();
    buffer->name(Carrot::sprintf("AS storage (%s)", createInfo.type == vk::AccelerationStructureTypeKHR::eBottomLevel ? "Bottom" : "Top"));

    as = driver.getLogicalDevice().createAccelerationStructureKHRUnique(createInfo, driver.getAllocationCallbacks());

    deviceAddress = GetVulkanDevice().getAccelerationStructureAddressKHR({.accelerationStructure = *as});
    ASByStartAddress.getOrCompute(deviceAddress, [&]() {
        return this;
    });
}

vk::AccelerationStructureKHR& Carrot::AccelerationStructure::getVulkanAS() {
    return *as;
}

Carrot::Buffer& Carrot::AccelerationStructure::getBuffer() {
    return *buffer;
}

const Carrot::Buffer& Carrot::AccelerationStructure::getBuffer() const {
    return *buffer;
}

Carrot::AccelerationStructure::~AccelerationStructure() {
    ASByStartAddress.remove(deviceAddress);
    deviceAddress = 0x0;
    GetVulkanDriver().deferDestroy(buffer->getDebugName()+"-as", std::move(as));
}
