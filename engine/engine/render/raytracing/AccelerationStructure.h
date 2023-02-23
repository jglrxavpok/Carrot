//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "engine/Engine.h"

namespace Carrot {
    class AccelerationStructure {
    public:
        static Carrot::Async::ParallelMap<vk::DeviceAddress, const Carrot::AccelerationStructure*> ASByStartAddress;

        explicit AccelerationStructure(VulkanDriver& engine, vk::AccelerationStructureCreateInfoKHR& createInfo);

        explicit AccelerationStructure(AccelerationStructure&&) = default;
        explicit AccelerationStructure(AccelerationStructure&) = delete;

        AccelerationStructure& operator=(AccelerationStructure&&) = default;
        AccelerationStructure& operator=(const AccelerationStructure&) = delete;

        virtual ~AccelerationStructure();

        vk::AccelerationStructureKHR& getVulkanAS();

        Buffer& getBuffer();
        const Buffer& getBuffer() const;

    private:
        std::unique_ptr<Buffer> buffer = nullptr;
        vk::UniqueAccelerationStructureKHR as{};
        vk::DeviceAddress deviceAddress = 0x0;
    };
}
