//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once

#include "RenderEye.h"
#include "Viewport.h"

namespace sol {
    class state;
}

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {
    struct Context {
        VulkanRenderer& renderer;
        Viewport& viewport;
        Eye eye = Eye::NoVR;
        size_t frameCount = -1;
        size_t swapchainIndex = -1;
        size_t lastSwapchainIndex = -1;

        vk::DescriptorSet getCameraDescriptorSet() const;

        /// Returns a copy of this RenderContext, with swapchainIndex = lastSwapchainIndex. Calling it again on the
        /// resulting object won't have any effect (outside of creating a new object)
        Context lastFrame() const;

        static void registerUsertype(sol::state& destination);
    };
}