//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once
#include <string>
#include <vector>
#include "engine/vulkan/includes.h"

constexpr unsigned int WINDOW_WIDTH = 1200;
constexpr unsigned int WINDOW_HEIGHT = 780;
constexpr const char* WINDOW_TITLE = "Carrot";
constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> VULKAN_VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation",
#ifndef NO_DEBUG
        "VK_LAYER_LUNARG_monitor",
#endif
};

const std::vector<const char*> VULKAN_DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NO_DEBUG
    constexpr bool USE_VULKAN_VALIDATION_LAYERS = false;
    constexpr bool USE_DEBUG_MARKERS = false;
#else
    const std::vector<const char*> VULKAN_DEBUG_EXTENSIONS = {
        VK_EXT_DEBUG_MARKER_EXTENSION_NAME
    };
    constexpr bool USE_VULKAN_VALIDATION_LAYERS = true;

    #ifdef DEBUG_MARKERS
        constexpr bool USE_DEBUG_MARKERS = true;
    #else
        constexpr bool USE_DEBUG_MARKERS = false;
    #endif
#endif