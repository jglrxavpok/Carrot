//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once

#include <core/async/ParallelMap.hpp>
#include <engine/render/resources/BufferAllocation.h>

namespace Carrot {
    class VulkanDriver;

    class AccelerationStructure {
    public:
        static Async::ParallelMap<vk::DeviceAddress, const Carrot::AccelerationStructure*> ASByStartAddress;

        explicit AccelerationStructure(VulkanDriver& engine, vk::AccelerationStructureCreateInfoKHR& createInfo);
        explicit AccelerationStructure(VulkanDriver& engine, vk::AccelerationStructureCreateInfoKHR& createInfo, Carrot::BufferAllocation&& preAllocatedMemory);

        AccelerationStructure(AccelerationStructure&&) = default;
        AccelerationStructure(const AccelerationStructure&) = delete;

        AccelerationStructure& operator=(AccelerationStructure&&) = default;
        AccelerationStructure& operator=(const AccelerationStructure&) = delete;

        virtual ~AccelerationStructure();

        vk::AccelerationStructureKHR& getVulkanAS();
        const vk::DeviceAddress& getDeviceAddress() const;

        BufferAllocation& getBuffer();
        const BufferAllocation& getBuffer() const;

    private:
        BufferAllocation buffer;
        vk::UniqueAccelerationStructureKHR as{};
        vk::DeviceAddress deviceAddress = 0x0;
    };
}
