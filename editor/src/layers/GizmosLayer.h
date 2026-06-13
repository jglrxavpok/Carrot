//
// Created by jglrxavpok on 16/02/2023.
//

#pragma once

#include <engine/io/actions/ActionSet.h>

#include "ISceneViewLayer.h"
#include <engine/render/resources/Texture.h>

namespace Peeler {

    class GizmosLayer: public ISceneViewLayer {
    public:
        explicit GizmosLayer(Application& editor);

        bool allowCameraMovement() const override;

        bool showLayersBelow() const override;

        bool allowSceneEntityPicking() const override;

        virtual void draw(const Carrot::Render::Context& renderContext, float startX, float startY) override final;

        void tick(double dt) override;

    private:
        Carrot::Render::Texture translateIcon;
        Carrot::Render::Texture rotateIcon;
        Carrot::Render::Texture scaleIcon;
        Carrot::Render::Texture scaleBoundsIcon;

        // ImGuizmo::OPERATION
        int gizmoOperation = 0;

        Carrot::IO::ActionSet shortcuts {"gizmo_shortcuts"};
        Carrot::IO::BoolInputAction translateMode{"translate"};
        Carrot::IO::BoolInputAction rotateMode{"rotate"};
        Carrot::IO::BoolInputAction scaleMode{"scale"};
        Carrot::IO::BoolInputAction nextMovementMode{"next_tool"};

        bool usingGizmo = false;
    };

} // Peeler
