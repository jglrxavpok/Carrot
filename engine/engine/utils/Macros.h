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
#define GetCapabilities() GetEngine().getCapabilities()