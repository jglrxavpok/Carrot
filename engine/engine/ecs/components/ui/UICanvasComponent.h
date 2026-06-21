//
// Created by jglrxavpok on 21/06/2026.
//

#pragma once
#include <engine/ecs/components/Component.h>
#include <engine/render/RenderPassData.h>

namespace Carrot::UI {
    BEGIN_COMPONENT(UICanvas)
        Render::TextureSize size;
        bool inWorld = false;
    END_COMPONENT
}

ADD_COMPONENT_ID(Carrot::UI, UICanvas)
