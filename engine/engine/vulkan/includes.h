//
// Created by jglrxavpok on 26/11/2020.
//

#pragma once

// This file allows to include everything needed for VulkanHpp to be configured the same way across the entire project

#undef VULKAN_HPP_DISABLE_ENHANCED_MODE
#define VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>