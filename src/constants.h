//
// Created by jglrxavpok on 21/11/2020.
//

#pragma once
#include <string>

constexpr unsigned int WINDOW_WIDTH = 1200;
constexpr unsigned int WINDOW_HEIGHT = 780;
constexpr const char* WINDOW_TITLE = "Carrot";

const std::vector<const char*> VULKAN_VALIDATION_LAYERS = {
        "VK_LAYER_KHRONOS_validation"
};

#ifdef NO_DEBUG
    constexpr bool USE_VULKAN_VALIDATION_LAYERS = false;
#else
    constexpr bool USE_VULKAN_VALIDATION_LAYERS = true;
#endif