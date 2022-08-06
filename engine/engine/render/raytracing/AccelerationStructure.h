//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "engine/Engine.h"

namespace Carrot {
    class AccelerationStructure {
    public:
        explicit AccelerationStructure(VulkanDriver& engine, vk::AccelerationStructureCreateInfoKHR& createInfo);

        explicit AccelerationStructure(AccelerationStructure&&) = default;
        explicit AccelerationStructure(AccelerationStructure&) = delete;

        AccelerationStructure& operator=(AccelerationStructure&&) = default;
        AccelerationStructure& operator=(const AccelerationStructure&) = delete;

        virtual ~AccelerationStructure();

        vk::AccelerationStructureKHR& getVulkanAS();

        Buffer& getBuffer();

    private:
        std::unique_ptr<Buffer> buffer = nullptr;
        vk::UniqueAccelerationStructureKHR as{};
    };
}
