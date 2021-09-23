//
// Created by jglrxavpok on 22/09/2021.
//

#include "RenderContext.h"
#include "Viewport.h"

namespace Carrot::Render {
    vk::DescriptorSet Context::getCameraDescriptorSet() const {
        return viewport.getCameraDescriptorSet(*this);
    }
}