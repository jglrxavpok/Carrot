//
// Created by jglrxavpok on 13/07/2021.
//

#pragma once

#include "engine/vulkan/includes.h"

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>
#include <openxr/openxr.h>

#undef XR_MSFT_scene_understanding
#undef XR_MSFT_scene_understanding_serialization
#include <openxr/openxr.hpp> // my build does not work with XR_MSFT_scene_understanding*