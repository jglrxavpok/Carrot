//
// Created by jglrxavpok on 26/09/2021.
//

#pragma once

#include "engine/render/RenderContext.h"

namespace Carrot::Render {
    /// Renderable object that can be embedded inside the default render graph passes of the engine
    struct BasicRenderable {
        virtual ~BasicRenderable() = default;

        virtual void onFrame(const Carrot::Render::Context& renderContext) {};
        virtual void renderOpaqueGBuffer(const Render::CompiledPass& pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const {};
        virtual void renderTransparentGBuffer(const Render::CompiledPass& pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands) const {};
    };
}