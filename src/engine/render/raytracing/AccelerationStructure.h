//
// Created by jglrxavpok on 30/12/2020.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "engine/Engine.h"

namespace Carrot {
    class AccelerationStructure {
    private:
        Engine& engine;
        unique_ptr<Buffer> buffer = nullptr;
        vk::UniqueAccelerationStructureKHR as{};

    public:
        explicit AccelerationStructure(Engine& engine, vk::AccelerationStructureCreateInfoKHR& createInfo);

        vk::AccelerationStructureKHR& getVulkanAS();
    };
}
