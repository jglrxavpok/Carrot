//
// Created by jglrxavpok on 16/02/2023.
//

#pragma once

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

    private:
        Carrot::Render::Texture translateIcon;
        Carrot::Render::Texture rotateIcon;
        Carrot::Render::Texture scaleIcon;

        bool usingGizmo = false;
    };

} // Peeler
