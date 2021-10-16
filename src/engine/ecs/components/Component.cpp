//
// Created by jglrxavpok on 20/02/2021.
//

#include <engine/render/RenderContext.h>
#include "Component.h"
#include <imgui/imgui.h>

namespace Carrot::ECS {
    void Component::drawInspector(const Carrot::Render::Context& renderContext, bool& shouldKeep, bool& modified) {
        shouldKeep = true;
        std::string s = getName();
        s += "##" + getEntity().getID().toString();
        if(ImGui::CollapsingHeader(s.c_str(), &shouldKeep)) {
            drawInspectorInternals(renderContext, modified);
        }

        if(!shouldKeep) {
            shouldKeep = false;
        }
    }
}

Carrot::ECS::ComponentLibrary& Carrot::ECS::getComponentLibrary() {
    static ComponentLibrary lib;
    return lib;
}