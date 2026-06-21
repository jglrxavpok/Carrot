//
// Created by jglrxavpok on 21/06/2026.
//

#pragma once
#include "ISceneViewLayer.h"

namespace Peeler {
    class CameraTypeLayer: public ISceneViewLayer {
    public:
        CameraTypeLayer(Application& editor): ISceneViewLayer(editor){};

        void draw(const Carrot::Render::Context& renderContext, float startX, float startY) override;
    };
} // Peeler