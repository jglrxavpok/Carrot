//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once
#include <string>
#include <vector>
#include "vulkan/includes.h"

constexpr unsigned int WINDOW_WIDTH = 1200;
constexpr unsigned int WINDOW_HEIGHT = 780;
constexpr const char* WINDOW_TITLE = "Carrot";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> VULKAN_VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> VULKAN_DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NO_DEBUG
    constexpr bool USE_VULKAN_VALIDATION_LAYERS = false;
#else
    constexpr bool USE_VULKAN_VALIDATION_LAYERS = true;
#endif