//
// Created by jglrxavpok on 01/10/2023.
//

#pragma once

#include "engine/render/InstanceData.h"
#include "engine/render/animation/AnimatedInstances.h"

namespace Carrot {
    class AnimatedInstances;
    class Model;
    class TaskHandle;
}

namespace Carrot::Render {
    /**
     * Represents a user-friendly interface to models with animations.
     * Supports instancing, via requestHandle
     */
    class AnimatedModel: public std::enable_shared_from_this<AnimatedModel> {
    public:
        constexpr static std::size_t MaxInstances = 128;

        class Handle {
        public:
            Handle(const std::shared_ptr<AnimatedModel>& parent);
            ~Handle();

            AnimatedInstanceData& getData();
            AnimatedModel& getParent();

        private:
            std::shared_ptr<AnimatedModel> parent;
            AnimatedInstanceData data; //< will be copied to GPU buffer each frame
        };

        explicit AnimatedModel(const std::shared_ptr<Model>& model);

        /**
         * Creates and register a new handle.
         * Handles are meant to be an easy way to add new instances of an AnimatedModel.
         */
        std::shared_ptr<Handle> requestHandle();

        /**
         * Updates and renders instances of this animated model
         */
        void onFrame(const Carrot::Render::Context& renderContext);

    private:
        std::shared_ptr<Model> model;
        AnimatedInstances animatedInstances;

        std::vector<std::shared_ptr<Handle>> handles;
    };

} // Carrot::Render
