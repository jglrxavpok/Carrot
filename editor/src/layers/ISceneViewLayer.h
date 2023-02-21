//
// Created by jglrxavpok on 16/02/2023.
//

#pragma once

namespace Carrot::Render {
    struct Context;
}

namespace Peeler {
    class Application;

    /**
     * Layers are stacked over one another and choose whether to use the behaviour of the layers below them
     */
    class ISceneViewLayer {
    public:
        explicit ISceneViewLayer(Application& editor): editor(editor) {};

        virtual ~ISceneViewLayer() = default;

    public:
        /**
         * Can the user move the camera while this layer is active?
         */
        virtual bool allowCameraMovement() const { return true; }

        /**
         * Can the user see & interact with the layers below this one?
         */
        virtual bool showLayersBelow() const { return true; }

        /**
         * Can the user select an entity with a left click inside the scene?
         */
        virtual bool allowSceneEntityPicking() const { return true; }

        virtual void draw(const Carrot::Render::Context& renderContext, float startX, float startY) = 0;

        virtual void tick(double dt) {};

    public:
        void remove();

    protected:
        Application& editor;
    };
}