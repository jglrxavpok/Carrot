//
// Created by jglrxavpok on 09/12/2024.
//

#pragma once

#include <engine/render/RenderPass.h>

namespace Carrot::Render {
    PassData::Lighting addLightingPasses(const Carrot::Render::PassData::GBuffer& opaqueData, Render::GraphBuilder& graph, const Render::TextureSize& framebufferSize);
}
