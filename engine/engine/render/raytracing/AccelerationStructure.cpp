//
// Created by jglrxavpok on 30/12/2020.
//

#include "AccelerationStructure.h"
#include <engine/render/resources/Buffer.h>
#include <engine/render/resources/ResourceAllocator.h>

Carrot::Async::ParallelMap<vk::DeviceAddress, const Carrot::AccelerationStructure*> Carrot::AccelerationStructure::ASByStartAddress;

Carrot::AccelerationStructure::AccelerationStructure(Carrot::VulkanDriver& driver,
                                                     vk::AccelerationStructureCreateInfoKHR& createInfo) {
    // allocate buffer to store AS
    buffer = GetResourceAllocator().allocateDeviceBuffer(createInfo.size,
                                                         vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress);

    createInfo.buffer = buffer.view.getVulkanBuffer();
    createInfo.offset = buffer.view.getStart();
    createInfo.size = buffer.view.getSize();
    buffer.name(Carrot::sprintf("AS storage (%s)", createInfo.type == vk::AccelerationStructureTypeKHR::eBottomLevel ? "Bottom" : "Top"));

    as = GetVulkanDriver().getLogicalDevice().createAccelerationStructureKHRUnique(createInfo, GetVulkanDriver().getAllocationCallbacks());

    deviceAddress = GetVulkanDevice().getAccelerationStructureAddressKHR({.accelerationStructure = *as});
    ASByStartAddress.getOrCompute(deviceAddress, [&]() {
        return this;
    });
}

vk::AccelerationStructureKHR& Carrot::AccelerationStructure::getVulkanAS() {
    return *as;
}

const vk::DeviceAddress& Carrot::AccelerationStructure::getDeviceAddress() const {
    return deviceAddress;
}

Carrot::BufferAllocation& Carrot::AccelerationStructure::getBuffer() {
    return buffer;
}

const Carrot::BufferAllocation& Carrot::AccelerationStructure::getBuffer() const {
    return buffer;
}

Carrot::AccelerationStructure::~AccelerationStructure() {
    ASByStartAddress.remove(deviceAddress);
    deviceAddress = 0x0;
    GetVulkanDriver().deferDestroy(buffer.getDebugName()+"-as", std::move(as), std::move(buffer));
}
