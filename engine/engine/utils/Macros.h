//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include <stdexcept>
#include <core/Macros.h>

#define GetEngine() Carrot::Engine::getInstance()
#define GetRenderer() GetEngine().getRenderer()
#define GetVulkanDriver() GetEngine().getVulkanDriver()
#define GetResourceAllocator() GetEngine().getResourceAllocator()
#define GetVulkanDevice() GetVulkanDriver().getLogicalDevice()
#define WaitDeviceIdle() GetVulkanDriver().waitDeviceIdle()
#define GetCapabilities() GetEngine().getCapabilities()
#define GetConfiguration() GetEngine().getConfiguration()
#define GetTaskScheduler() GetEngine().getTaskScheduler()

#undef GetVFS
#define GetVFS() GetEngine().getVFS()

#define GetPhysics() Carrot::Physics::PhysicsSystem::getInstance()