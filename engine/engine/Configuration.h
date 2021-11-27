//
// Created by jglrxavpok on 11/07/2021.
//

#pragma once
#ifdef ENABLE_VR
#include "engine/vr/includes.h"
#endif

#include "engine/constants.h"
#include "core/io/Resource.h"
#include <optional>

namespace Carrot {
    struct Configuration {
        bool useRaytracing = true;
        bool runInVR = false;

        const char* engineName = "Carrot";

        std::string applicationName = WINDOW_TITLE;
        std::uint32_t applicationVersion = VK_MAKE_VERSION(0, 1, 0);
        std::uint32_t engineVersion = VK_MAKE_VERSION(0, 1, 0);
        std::uint32_t vulkanApiVersion = VK_API_VERSION_1_2;

#ifdef ENABLE_VR
        std::uint64_t openXRApiVersion = XR_CURRENT_API_VERSION;
#endif

        std::optional<Carrot::IO::Resource> icon16;
        std::optional<Carrot::IO::Resource> icon32;
        std::optional<Carrot::IO::Resource> icon64;
        std::optional<Carrot::IO::Resource> icon128;

    };
}